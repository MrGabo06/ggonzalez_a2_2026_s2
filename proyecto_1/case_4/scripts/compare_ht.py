#!/usr/bin/env python3
"""compare_ht.py - compara las mediciones con Hyper-Threading ENCENDIDO y APAGADO
(el enunciado pide la comparacion de SMT activado/desactivado desde la BIOS).

Lee los dos CSV oficiales que genera bench.py:
    results/ht_on/benchmark/bench_oficial.csv
    results/ht_off/benchmark/bench_oficial.csv
y escribe en results/comparacion_ht/:
    comparacion_ht.csv   tabla: speedup (+/- IC95%) de cada configuracion con HT on y off
    comparacion_ht.png   graficas: SMT por hardware vs por software (2 hilos) y escalabilidad de cmp

El enunciado (4.1.c) pide aislar el efecto de SMT comparando contra la ejecucion con SMT apagado.
Con HT apagado no existen CPUs hermanas, asi que el SMT por hardware (`smt`) solo se mide con HT
encendido; la aproximacion por software es `smt_shared` (los mismos 2 hilos sobre UNA sola CPU
logica, sobre-suscripcion pura) y existe en ambos modos. `smt_ctrl` (2 nucleos distintos) es el
control en ambos modos.

Como cada modo se mide en una sesion distinta (hay que reiniciar a la BIOS), NO se comparan
tiempos absolutos sino el speedup de cada sesion contra SU PROPIO sequential. Dos speedups se
consideran distintos si sus intervalos de confianza al 95% no se solapan. Un IC solo recoge el
ruido dentro de una sesion, no la variacion entre sesiones: una diferencia chica (pocos %) debe
leerse con cautela.

Uso (desde proyecto_1/case_4):
    python3 scripts/compare_ht.py
    python3 scripts/compare_ht.py --on ruta_on.csv --off ruta_off.csv
"""
import argparse
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze import group_by_config, load_rows, mean_ci95, speedup_ci  # noqa: E402

CASE4_DIR = Path(__file__).resolve().parent.parent
DEFAULT_ON = CASE4_DIR / "results" / "ht_on" / "benchmark" / "bench_oficial.csv"
DEFAULT_OFF = CASE4_DIR / "results" / "ht_off" / "benchmark" / "bench_oficial.csv"
DEFAULT_OUT = CASE4_DIR / "results" / "comparacion_ht"

ENGINES = ["cmp", "smt_ctrl", "smt", "smt_shared", "coarseGrained", "fineGrained"]
INK, MUTED = "#0b0b0b", "#52514e"
COLOR_ON, COLOR_OFF = "#2a78d6", "#d9a400"


def session_metrics(csv_path: Path):
    """{(engine, threads): dict(time, speedup, lo, hi)} + tiempo de sequential de esa sesion."""
    groups = group_by_config(load_rows(csv_path))
    seq = groups.get(("sequential", 1))
    if not seq:
        sys.exit(f"ERROR: {csv_path} no tiene filas de sequential")
    seq_mean, _std, seq_sem, _ci = mean_ci95(seq)
    out = {}
    for (engine, threads), values in groups.items():
        if engine == "sequential":
            continue
        mean, _std, sem, _ci = mean_ci95(values)
        s, (lo, hi) = speedup_ci(seq_mean, seq_sem, mean, sem)
        out[(engine, threads)] = dict(time=mean, sem=sem, speedup=s, lo=lo, hi=hi, n=len(values))
    return out, seq_mean


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--on", type=Path, default=DEFAULT_ON, help="CSV con HT encendido")
    ap.add_argument("--off", type=Path, default=DEFAULT_OFF, help="CSV con HT apagado")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT, help="carpeta de salida")
    args = ap.parse_args()

    for p in (args.on, args.off):
        if not p.exists():
            sys.exit(f"ERROR: no existe {p}. Corre `make bench-oficial` en ese modo primero.")

    on, seq_on = session_metrics(args.on)
    off, seq_off = session_metrics(args.off)
    args.out.mkdir(parents=True, exist_ok=True)

    print("=== Hyper-Threading encendido vs apagado (speedup contra el sequential de cada sesion) ===")
    print(f"sequential: HT on {seq_on * 1000:.2f} ms | HT off {seq_off * 1000:.2f} ms "
          f"(diferencia entre sesiones: {100 * (seq_off / seq_on - 1):+.1f}%)\n")
    header = f"{'motor':<14}{'hilos':>5}  {'HT on':>16}  {'HT off':>16}  {'off vs on':>10}  conclusion"
    print(header)
    print("-" * len(header))

    rows = []
    keys = sorted(set(on) | set(off), key=lambda k: (ENGINES.index(k[0]) if k[0] in ENGINES else 99, k[1]))
    for key in keys:
        a, b = on.get(key), off.get(key)
        engine, threads = key
        fa = f"{a['speedup']:.3f} +/- {(a['hi'] - a['lo']) / 2:.3f}" if a else "-"
        fb = f"{b['speedup']:.3f} +/- {(b['hi'] - b['lo']) / 2:.3f}" if b else "-"
        if a and b:
            diff = 100 * (b["speedup"] / a["speedup"] - 1)
            overlap = not (a["hi"] < b["lo"] or b["hi"] < a["lo"])
            concl = "sin diferencia clara (IC se solapan)" if overlap else \
                ("HT off MAS lento" if diff < 0 else "HT off mas rapido")
            fd = f"{diff:+.1f}%"
        else:
            diff, overlap = None, None
            concl = "solo existe con HT on (necesita hilos hermanos)" if a else "solo con HT off"
            fd = "-"
        print(f"{engine:<14}{threads:>5}  {fa:>16}  {fb:>16}  {fd:>10}  {concl}")
        rows.append({
            "engine": engine, "threads": threads,
            "on_time_s": f"{a['time']:.9f}" if a else "", "on_speedup": f"{a['speedup']:.4f}" if a else "",
            "on_ci_low": f"{a['lo']:.4f}" if a else "", "on_ci_high": f"{a['hi']:.4f}" if a else "",
            "off_time_s": f"{b['time']:.9f}" if b else "", "off_speedup": f"{b['speedup']:.4f}" if b else "",
            "off_ci_low": f"{b['lo']:.4f}" if b else "", "off_ci_high": f"{b['hi']:.4f}" if b else "",
            "off_vs_on_pct": f"{diff:.2f}" if diff is not None else "",
            "ci_overlap": "" if overlap is None else ("si" if overlap else "no"),
        })

    csv_path = args.out / "comparacion_ht.csv"
    with csv_path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"\ntabla guardada en {csv_path}")
    # ---- efecto de SMT: hardware (hermanas) vs software (misma CPU) vs 2 nucleos
    print("\n=== Efecto de SMT (2 hilos): hardware vs software ===")
    smt_on, shared_on, ctrl_on = on.get(("smt", 2)), on.get(("smt_shared", 2)), on.get(("smt_ctrl", 2))
    shared_off, ctrl_off = off.get(("smt_shared", 2)), off.get(("smt_ctrl", 2))
    if smt_on and shared_on:
        ratio, ci = speedup_ci(shared_on["time"], shared_on["sem"], smt_on["time"], smt_on["sem"])
        print(f"  HT on : smt (hermanas) vs smt_shared (misma CPU): {ratio:.3f}x mas rapido "
              f"(IC95% [{ci[0]:.3f}, {ci[1]:.3f}]) -> el 2do hilo en el mismo nucleo aporta {100 * (ratio - 1):.0f}%")
        if ctrl_on:
            gain_smt = shared_on["time"] / smt_on["time"] - 1
            gain_ctrl = shared_on["time"] / ctrl_on["time"] - 1
            print(f"  HT on : un 2do nucleo completo aporta {100 * gain_ctrl:.0f}%; el SMT por hardware entrega "
                  f"{100 * gain_smt / gain_ctrl:.0f}% de eso")
    if shared_off and ctrl_off:
        ratio, ci = speedup_ci(shared_off["time"], shared_off["sem"], ctrl_off["time"], ctrl_off["sem"])
        print(f"  HT off: smt_ctrl (2 nucleos) vs smt_shared (misma CPU): {ratio:.3f}x mas rapido "
              f"(IC95% [{ci[0]:.3f}, {ci[1]:.3f}]); sin hermanas, el SMT por hardware no existe")
    if shared_on and shared_off:
        d = 100 * (shared_off["time"] / shared_on["time"] - 1)
        print(f"  control: smt_shared tarda {shared_on['time'] * 1000:.2f} ms (HT on) y {shared_off['time'] * 1000:.2f} ms "
              f"(HT off), {d:+.1f}% -> deberia ser ~0 (no usa hermanas)")
    print("Nota: el IC solo recoge el ruido de cada sesion; diferencias de pocos % entre sesiones deben leerse con cautela.")

    # ------------------------------------------------------------------ graficas
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axb = plt.subplots(1, 1, figsize=(8.6, 5.6))
    fig.patch.set_facecolor("white")

    # B) speedup por configuracion (barras HT on vs off)
    chosen = [("smt_shared", 2), ("smt", 2), ("smt_ctrl", 2)]
    chosen = [k for k in chosen if k in on or k in off]
    names = {"smt_shared": "smt_shared\n2 hilos, misma CPU\n(SMT por software)",
             "smt": "smt\n2 hilos hermanos\n(SMT por hardware)",
             "smt_ctrl": "smt_ctrl\n2 hilos, 2 nucleos\n(control)"}
    labels = [names.get(e, f"{e}\n{t} hilos") for e, t in chosen]
    pos = list(range(len(chosen)))
    w = 0.38
    for i, key in enumerate(chosen):
        for shift, d, color, hatch in ((-w / 2, on, COLOR_ON, None), (w / 2, off, COLOR_OFF, "//")):
            v = d.get(key)
            if not v:
                continue
            axb.bar(i + shift, v["speedup"], w, color=color, hatch=hatch, edgecolor="white" if hatch is None else "#7a5d00",
                    lw=0.8, zorder=3)
            axb.errorbar(i + shift, v["speedup"], yerr=(v["hi"] - v["lo"]) / 2, color=INK, capsize=3, lw=1.1, zorder=4)
            axb.text(i + shift, v["hi"] + 0.07, f"{v['speedup']:.2f}", ha="center", va="bottom", fontsize=8.5, color=INK)
        if key not in off:
            axb.text(i + w / 2, 0.08, "no existe\n(sin hermanas)", ha="center", va="bottom", fontsize=8, color=MUTED)
        if key not in on:
            axb.text(i - w / 2, 0.08, "no\nexiste", ha="center", va="bottom", fontsize=8, color=MUTED)
    axb.set_xticks(pos)
    axb.set_xticklabels(labels, fontsize=9)
    axb.axhline(1.0, color="#9a9893", lw=0.8, zorder=1)
    axb.set_ylabel("speedup contra sequential de la sesion", color=INK)
    axb.set_title("SMT por hardware vs por software (2 hilos)", loc="left", color=INK, fontsize=12)
    axb.set_ylim(0, 2.5)
    axb.grid(axis="y", color="#e6e5e0", lw=0.8, zorder=0)
    axb.set_axisbelow(True)
    from matplotlib.patches import Patch
    axb.legend(handles=[Patch(facecolor=COLOR_ON, label="HT encendido"),
                        Patch(facecolor=COLOR_OFF, hatch="//", edgecolor="#7a5d00", label="HT apagado")],
               loc="upper left", frameon=False, fontsize=9)
    for ax in (axb,):
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        for side in ("left", "bottom"):
            ax.spines[side].set_color("#9a9893")
        ax.tick_params(colors=MUTED)

    fig.suptitle("Hyper-Threading encendido vs apagado (barras de error: IC95%)", x=0.01, ha="left", fontsize=13, color=INK)
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    png_path = args.out / "comparacion_ht.png"
    fig.savefig(png_path, dpi=160, facecolor="white", bbox_inches="tight")
    plt.close(fig)
    print(f"grafica guardada en {png_path}")


if __name__ == "__main__":
    main()
