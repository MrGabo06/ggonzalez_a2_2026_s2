#!/usr/bin/env python3
"""perf_report.py - convierte las capturas de `make perf` (results/perf/*.txt)
en una tabla resumen (CSV + consola) y dos graficas para el paper.

Que calcula, por motor (cada captura = REPS corridas seguidas bajo un `perf stat -a -A`):
  - ms por corrida   : tiempo total de la captura / REPS (control: debe coincidir con bench.py).
  - instrucciones    : total del sistema; deberia ser ~igual en todos los motores
                       (todos hacen el mismo trabajo, solo cambia como se reparte).
  - CPUs de trabajo  : las CPUs logicas que se llevaron >= 15% de los ciclos del motor. Asi el
                       ruido de procesos de fondo (que perf -a tambien cuenta) no ensucia el IPC.
  - IPC              : instrucciones / ciclos en esas CPUs (trabajo util por ciclo de reloj).
  - % stalls         : ciclos sin ejecutar nada util / ciclos, en esas CPUs.
  - smt / smt_ctrl   : ademas, instrucciones por ciclo de un NUCLEO FISICO (en smt, sus 2 hilos
                       hermanos sumados; en smt_ctrl, el promedio de sus 2 nucleos) y una estimacion
                       de cuanto tiempo corrio un hilo SIN su hermano activo
                       (evento cpu_clk_unhalted.one_thread_active).

Uso (desde proyecto_1/case_4, despues de `make perf`):
    python3 scripts/perf_report.py        # lee y escribe en results/ht_<on|off>/perfilado/
    python3 scripts/perf_report.py --dir results/ht_on/perfilado/capturas --out results/ht_on/perfilado/perf
"""
import argparse
import csv
import re
import statistics
import sys
from pathlib import Path

CASE4_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ht_mode  # noqa: E402  (deteccion de Hyper-Threading y carpetas de results/)
ENGINES = ["sequential", "fineGrained", "coarseGrained", "cmp", "smt", "smt_ctrl"]
WORK_SHARE = 0.15  # una CPU cuenta como "de trabajo" si tiene >= 15% de los ciclos del motor

# Mismos colores que la grafica de escalabilidad de analyze.py (paleta validada para daltonismo).
COLORS = {
    "sequential": "#7a7973", "fineGrained": "#52514e", "coarseGrained": "#4a3aa7",
    "cmp": "#2a78d6", "smt": "#eb6834", "smt_ctrl": "#1baf7a",
}
INK, MUTED = "#0b0b0b", "#52514e"


def parse_capture(path: Path) -> dict:
    """Lee un results/perf/<motor>.txt. Devuelve eventos por CPU, tiempo y REPS."""
    text = path.read_text()
    events: dict[str, dict[int, int]] = {}
    # Linea de perf: "CPU3   1.946.678.912   cycles". El separador de miles depende del
    # idioma del sistema (punto o coma): se quitan ambos. "<not counted>" no matchea y se ignora.
    for m in re.finditer(r"^CPU(\d+)\s+([\d.,]+)\s+(\S+)", text, re.M):
        events.setdefault(m.group(3), {})[int(m.group(1))] = int(re.sub(r"[.,]", "", m.group(2)))
    el = re.search(r"([\d.,]+)\s+seconds time elapsed", text)
    elapsed = float(el.group(1).replace(",", ".")) if el else None  # aqui la coma es decimal
    reps = re.search(r"repetido (\d+) veces", text)
    cmd = re.search(r"# comando.*?: (.*)", text)
    return {"events": events, "elapsed": elapsed, "reps": int(reps.group(1)) if reps else None,
            "cmd": cmd.group(1).strip() if cmd else ""}


def cpus_from_cmd(cmd: str):
    """smt/smt_ctrl se lanzan con la lista de CPUs como ultimo argumento (ej. '0,4')."""
    last = cmd.split("(repetido")[0].split()[-1] if cmd else ""
    return [int(x) for x in last.split(",")] if re.fullmatch(r"\d+(,\d+)+", last) else None


def summarize(name: str, cap: dict, ref_ratio) -> dict:
    ev = cap["events"]
    cyc, ins = ev.get("cycles", {}), ev.get("instructions", {})
    stl = ev.get("cycle_activity.stalls_total", {})
    one = ev.get("cpu_clk_unhalted.one_thread_active", {})
    tot_c = sum(cyc.values())
    work = sorted(c for c in cyc if cyc[c] >= WORK_SHARE * tot_c)
    w_c = sum(cyc[c] for c in work)
    row = {
        "motor": name,
        "ms_por_corrida": (cap["elapsed"] / cap["reps"] * 1000) if cap["elapsed"] and cap["reps"] else None,
        "ciclos_G": tot_c / 1e9,
        "instrucciones_G": sum(ins.values()) / 1e9,
        "cpus_de_trabajo": ",".join(map(str, work)),
        "ciclos_en_trabajo_pct": 100 * w_c / tot_c,
        "ipc": sum(ins[c] for c in work) / w_c,
        "stalls_pct": 100 * sum(stl[c] for c in work) / w_c if stl else None,
        "ipc_nucleo": None,
        "sin_hermano_pct": None,
    }
    pair = cpus_from_cmd(cap["cmd"]) if name in ("smt", "smt_ctrl") else None
    if pair:
        # Instrucciones por ciclo de UN NUCLEO FISICO. smt: las 2 CPUs son hermanas (comparten un
        # nucleo), asi que se suman sus instrucciones y se divide entre sus ciclos promedio (corren
        # a la vez). smt_ctrl: cada CPU es un nucleo distinto, asi que es el IPC promedio por nucleo.
        if name == "smt":
            row["ipc_nucleo"] = sum(ins[c] for c in pair) / statistics.mean(cyc[c] for c in pair)
        else:
            row["ipc_nucleo"] = statistics.mean(ins[c] / cyc[c] for c in pair)
        if one and ref_ratio:
            # one_thread_active parece contar en reloj de cristal (~1 de cada ~115 ciclos), no en
            # ciclos de nucleo; ref_ratio se calibra con cmp, donde cada hilo esta solo en su nucleo.
            row["sin_hermano_pct"] = statistics.mean(
                min(100.0, 100 * one[c] * ref_ratio / cyc[c]) for c in pair)
    return row


def calibrate_ratio(cap: dict):
    """ciclos / one_thread_active en cmp (hilos solos en su nucleo): convierte ese evento a ciclos."""
    ev = cap["events"]
    cyc, one = ev.get("cycles", {}), ev.get("cpu_clk_unhalted.one_thread_active", {})
    tot = sum(cyc.values())
    r = [cyc[c] / one[c] for c in cyc if c in one and one[c] and cyc[c] >= WORK_SHARE * tot]
    return statistics.median(r) if r else None


def fmt(v, nd=2, suf=""):
    return "-" if v is None else f"{v:.{nd}f}{suf}"


def make_plots(caps: dict, rows: dict, out_prefix: Path):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("\n(matplotlib no esta instalado - me salto las graficas; 'pip3 install matplotlib')", file=sys.stderr)
        return
    names = [n for n in ENGINES if n in caps]

    # --- Grafica 1: reparto de ciclos por CPU logica (un panel por motor) -------------------
    ncols = 3
    nrows = (len(names) + ncols - 1) // ncols
    fig, axes = plt.subplots(nrows, ncols, figsize=(11, 3.1 * nrows), sharey=True)
    axes = axes.flatten() if hasattr(axes, "flatten") else [axes]
    for ax, n in zip(axes, names):
        cyc = caps[n]["events"]["cycles"]
        tot = sum(cyc.values())
        cpus = sorted(cyc)
        ax.bar(cpus, [100 * cyc[c] / tot for c in cpus], color=COLORS[n], width=0.7)
        ms = rows[n]["ms_por_corrida"]
        ax.set_title(f"{n}" + (f"  ({ms:.1f} ms/corrida)" if ms else ""), fontsize=10.5, color=INK, loc="left")
        ax.set_xticks(cpus)
        ax.set_xlabel("CPU logica", fontsize=9, color=MUTED)
        ax.grid(axis="y", color="#e6e5e0", lw=0.8)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        for side in ("left", "bottom"):
            ax.spines[side].set_color("#9a9893")
        ax.tick_params(colors=MUTED, labelsize=9)
        for c in cpus:  # etiqueta solo donde hay trabajo real
            p = 100 * cyc[c] / tot
            if p >= 8:
                ax.text(c, p + 1.5, f"{p:.0f}%", ha="center", fontsize=8.5, color=INK)
    for ax in axes[len(names):]:
        ax.set_visible(False)
    for i in range(0, len(names), ncols):
        axes[i].set_ylabel("% de los ciclos del motor", fontsize=9, color=MUTED)
    axes[0].set_ylim(0, 100)
    fig.suptitle("Reparto de ciclos por CPU logica (perf stat -a -A)", fontsize=12.5, color=INK, x=0.01, ha="left")
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    p1 = out_prefix.with_name(out_prefix.name + "_ciclos_por_cpu.png")
    fig.savefig(p1, dpi=170, facecolor="white", bbox_inches="tight")
    plt.close(fig)
    print(f"grafica guardada en {p1}")

    # --- Grafica 2: IPC y % de stalls por motor (solo CPUs de trabajo) ---------------------
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11, 4.4))
    xs = list(range(len(names)))
    for ax, key, title, suf in ((a1, "ipc", "Instrucciones por ciclo (IPC) por hilo", ""),
                                (a2, "stalls_pct", "Ciclos en stall (sin trabajo util)", "%")):
        vals = [rows[n][key] or 0 for n in names]
        ax.bar(xs, vals, color=[COLORS[n] for n in names], width=0.65)
        for x, v in zip(xs, vals):
            ax.text(x, v * 1.02 + (0.03 if key == "ipc" else 0.3), f"{v:.2f}" if key == "ipc" else f"{v:.1f}%",
                    ha="center", fontsize=9.5, color=INK)
        ax.set_xticks(xs)
        ax.set_xticklabels(names, rotation=30, ha="right", fontsize=9.5, color=INK)
        ax.set_title(title, fontsize=11, color=INK, loc="left")
        ax.grid(axis="y", color="#e6e5e0", lw=0.8)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        for side in ("left", "bottom"):
            ax.spines[side].set_color("#9a9893")
        ax.tick_params(colors=MUTED, labelsize=9.5)
    a1.set_ylim(0, max(rows[n]["ipc"] for n in names) * 1.25)
    a2.set_ylim(0, max(rows[n]["stalls_pct"] or 0 for n in names) * 1.25)
    note = "IPC por hilo: instrucciones por ciclo de cada CPU de trabajo."
    if "smt" in rows and rows["smt"]["ipc_nucleo"]:
        i = names.index("smt")
        # dentro de la barra de smt: lo que rinde el NUCLEO con sus 2 hilos juntos
        a1.text(i, rows["smt"]["ipc"] / 2, f"nucleo:\n{rows['smt']['ipc_nucleo']:.2f}",
                ha="center", va="center", fontsize=9, color="white", fontweight="bold")
        note += (f"  En smt, los 2 hilos hermanos juntos ejecutan {rows['smt']['ipc_nucleo']:.2f} instr/ciclo "
                 f"en su nucleo.")
    fig.text(0.01, -0.01, note, fontsize=9, color=MUTED, ha="left", va="top")
    fig.suptitle("Que hizo cada CPU de trabajo (perf)", fontsize=12.5, color=INK, x=0.01, ha="left")
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    p2 = out_prefix.with_name(out_prefix.name + "_ipc_stalls.png")
    fig.savefig(p2, dpi=170, facecolor="white", bbox_inches="tight")
    plt.close(fig)
    print(f"grafica guardada en {p2}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", default=None, help="carpeta con las capturas de `make perf` (default: results/ht_<on|off>/perfilado/capturas)")
    ap.add_argument("--out", default=None, help="prefijo de salida: genera <prefijo>_resumen.csv y <prefijo>_*.png (default: results/ht_<on|off>/perfilado/perf)")
    args = ap.parse_args()

    mode, source = ht_mode.detect_mode()
    perfilado = ht_mode.results_root(mode) / "perfilado"
    perf_dir = CASE4_DIR / args.dir if args.dir else perfilado / "capturas"
    out_prefix = CASE4_DIR / args.out if args.out else perfilado / "perf"
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    print(f"Hyper-Threading: {mode} (detectado por {source}) -> leyendo {perf_dir}", file=sys.stderr)
    caps = {}
    for name in ENGINES:
        f = perf_dir / f"{name}.txt"
        if f.exists():
            caps[name] = parse_capture(f)
    if not caps:
        sys.exit(f"No encontre capturas en {perf_dir}. Corre primero `make perf` (Linux fisico).")
    missing = [n for n in ENGINES if n not in caps]
    if missing:
        print(f"aviso: faltan capturas de {', '.join(missing)}", file=sys.stderr)

    ref_ratio = calibrate_ratio(caps["cmp"]) if "cmp" in caps else None
    rows = {n: summarize(n, c, ref_ratio) for n, c in caps.items()}

    print(f"{'motor':<14}{'ms/corrida':>11}{'instr (G)':>11}{'ciclos (G)':>11}{'CPUs de trabajo':>17}{'IPC':>7}{'stalls':>8}{'IPC/nucleo':>12}{'sin hermano':>13}")
    for n in ENGINES:
        if n not in rows:
            continue
        r = rows[n]
        print(f"{n:<14}{fmt(r['ms_por_corrida'], 1):>11}{r['instrucciones_G']:>11.2f}{r['ciclos_G']:>11.2f}"
              f"{r['cpus_de_trabajo']:>17}{r['ipc']:>7.2f}{fmt(r['stalls_pct'], 1, '%'):>8}"
              f"{fmt(r['ipc_nucleo']):>12}{fmt(r['sin_hermano_pct'], 0, '%'):>13}")
    print("\nIPC y stalls se calculan solo en las CPUs de trabajo (>= 15% de los ciclos del motor).")
    print("'IPC/nucleo' = instrucciones por ciclo de UN nucleo fisico (smt: sus 2 hilos hermanos sumados; smt_ctrl: promedio de sus 2 nucleos).")
    print("'sin hermano' = % estimado del tiempo que un hilo corrio sin su hermano activo (smt ~0% = siempre compartiendo).")

    csv_path = out_prefix.with_name(out_prefix.name + "_resumen.csv")
    with csv_path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(next(iter(rows.values())).keys()))
        w.writeheader()
        w.writerows(rows[n] for n in ENGINES if n in rows)
    print(f"\nresumen guardado en {csv_path}")
    make_plots(caps, rows, out_prefix)


if __name__ == "__main__":
    main()
