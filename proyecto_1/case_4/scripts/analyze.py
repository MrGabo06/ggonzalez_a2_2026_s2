#!/usr/bin/env python3
"""analyze.py - calcula speedup, eficiencia, escalabilidad, intervalo de
confianza 95%, convergencia, y compara el speedup medido contra la
prediccion teorica de la Ley de Amdahl, a partir del CSV que genera
bench.py.

fs (fraccion paralelizable) se estima por minimos cuadrados a partir de
los propios datos medidos, no a ojo del codigo: para cada cantidad de
hilos n>1 se despeja fs de la ecuacion de Amdahl S(n) = 1/(fs + (1-fs)/n),
y se ajusta por minimos cuadrados (recta forzada por el origen). Es mas
objetivo y facil de justificar en la defensa que estimarlo viendo el codigo.

Amdahl supone UN procesador real por hilo. Por eso el ajuste usa solo los
puntos con 1 < n <= nucleos fisicos; los puntos con mas hilos que nucleos
fisicos ya usan SMT (hilos logicos que comparten nucleo), que no sigue la
formula. Esos puntos se reportan como "smt" y la curva se extrapola solo para
compararla con lo medido, sin entrar al ajuste.

Los nucleos fisicos se detectan con lscpu (o se fuerzan con --cores N).

Uso:
    python3 scripts/analyze.py results/bench_oficial.csv
    python3 scripts/analyze.py results/bench_oficial.csv --out results/analisis
    python3 scripts/analyze.py results/bench_oficial.csv --cores 4
"""
import argparse
import csv
import math
import statistics
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

Z95 = 1.96  # aproximacion normal; con n=200 por configuracion sobra (regla usual: n>=30)


def load_rows(csv_path: Path):
    rows = []
    with csv_path.open() as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append({
                "engine": r["engine"],
                "threads": int(r["threads"]),
                "run": int(r["run"]),
                "time_sec": float(r["time_sec"]),
            })
    return rows


def group_by_config(rows):
    groups = defaultdict(list)
    for r in rows:
        groups[(r["engine"], r["threads"])].append(r["time_sec"])
    return groups


def mean_ci95(values):
    n = len(values)
    mean = statistics.mean(values)
    std = statistics.stdev(values) if n > 1 else 0.0
    sem = std / math.sqrt(n) if n > 0 else 0.0
    half = Z95 * sem
    return mean, std, sem, (mean - half, mean + half)


def speedup_ci(seq_mean, seq_sem, eng_mean, eng_sem):
    """Propagacion de error (formula delta) para S = seq/eng, asumiendo
    seq y eng independientes entre si (son corridas distintas)."""
    speedup = seq_mean / eng_mean
    rel = math.sqrt((seq_sem / seq_mean) ** 2 + (eng_sem / eng_mean) ** 2) if seq_mean and eng_mean else 0.0
    half = Z95 * speedup * rel
    return speedup, (speedup - half, speedup + half)


def fit_fs(threads_speedups):
    """threads_speedups: lista de (n, speedup_medido) con n>1. fs = fraccion
    PARALELIZABLE (la que se beneficia de mas hilos): S(n) = 1/(fs/n + (1-fs)).
    Despejando: 1 - 1/S(n) = fs * (1 - 1/n)  ->  ajuste por minimos cuadrados,
    recta forzada por el origen (x = 1-1/n, y = 1-1/S)."""
    num = 0.0
    den = 0.0
    for n, s in threads_speedups:
        if n <= 1 or s <= 0:
            continue
        x = 1 - 1 / n
        y = 1 - 1 / s
        num += x * y
        den += x * x
    if den == 0:
        return None
    fs = num / den
    return max(0.0, min(1.0, fs))  # recortar a [0,1]: un ajuste con ruido puede salirse


def detect_physical_cores():
    """Cantidad de nucleos fisicos segun lscpu (pares unicos CORE,SOCKET).
    Devuelve None si no hay lscpu (ej. macOS) o no se pudo leer."""
    try:
        proc = subprocess.run(["lscpu", "-p=CORE,SOCKET"], capture_output=True, text=True, timeout=10)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0:
        return None
    cores = {line.strip() for line in proc.stdout.splitlines() if line.strip() and not line.startswith("#")}
    return len(cores) or None


def fit_points(pairs, cores):
    """Puntos (n, speedup) que entran al ajuste de Amdahl: 1 < n <= cores.
    Si no se conoce `cores`, entran todos los n>1 (comportamiento anterior)."""
    return [(n, s) for n, s in pairs if n > 1 and (cores is None or n <= cores)]


def amdahl_speedup(fs, n):
    """fs = fraccion paralelizable. fs=1 -> speedup lineal ideal; fs=0 -> sin
    speedup (plano), como fineGrained."""
    return 1 / (fs / n + (1 - fs))


def convergence(values):
    """Promedio acumulado en checkpoints relativos (5%,10%,25%,50%,75%,100%
    de las corridas), comparado contra el promedio final, para ver en que
    punto deja de moverse significativamente."""
    n_total = len(values)
    final_mean = statistics.mean(values)
    checkpoints = sorted(set(max(1, round(n_total * f)) for f in (0.05, 0.10, 0.25, 0.50, 0.75, 1.0)))
    out = []
    for c in checkpoints:
        running = statistics.mean(values[:c])
        pct_diff = 100 * abs(running - final_mean) / final_mean if final_mean else 0.0
        out.append((c, running, pct_diff))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv_path", type=str, help="CSV generado por bench.py (ej. results/bench_oficial.csv)")
    ap.add_argument("--out", type=str, default=None, help="prefijo de salida (default: junto al CSV de entrada)")
    ap.add_argument("--cores", type=int, default=None, help="nucleos fisicos de la maquina (default: autodeteccion con lscpu)")
    args = ap.parse_args()

    cores = args.cores or detect_physical_cores()
    if cores:
        print(f"nucleos fisicos: {cores} (el ajuste de Amdahl usa solo hilos <= {cores}; mas alla es SMT)")
    else:
        print("AVISO: no pude detectar los nucleos fisicos (usa --cores N); el ajuste de Amdahl usara TODOS los hilos", file=sys.stderr)

    csv_path = Path(args.csv_path).resolve()
    rows = load_rows(csv_path)
    groups = group_by_config(rows)

    out_prefix = Path(args.out).resolve() if args.out else csv_path.parent / csv_path.stem
    out_prefix.parent.mkdir(parents=True, exist_ok=True)

    seq_values = groups.get(("sequential", 1))
    if not seq_values:
        print("ERROR: no encontre filas de 'sequential' (threads=1) en el CSV", file=sys.stderr)
        sys.exit(1)
    seq_mean, seq_std, seq_sem, seq_ci = mean_ci95(seq_values)

    metrics_rows = []
    speedups_by_engine = defaultdict(list)

    for (engine, threads), values in sorted(groups.items()):
        n = len(values)
        mean, std, sem, ci = mean_ci95(values)
        if engine == "sequential":
            speedup, sp_ci = 1.0, (1.0, 1.0)
        else:
            speedup, sp_ci = speedup_ci(seq_mean, seq_sem, mean, sem)
            speedups_by_engine[engine].append((threads, speedup))
        efficiency = speedup / threads if threads else None
        metrics_rows.append({
            "engine": engine, "threads": threads, "n": n,
            "mean_time_sec": mean, "std_time_sec": std,
            "ci95_low": ci[0], "ci95_high": ci[1],
            "speedup": speedup, "speedup_ci95_low": sp_ci[0], "speedup_ci95_high": sp_ci[1],
            "efficiency": efficiency,
        })

    metrics_csv = out_prefix.with_name(out_prefix.name + "_metrics.csv")
    with metrics_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(metrics_rows[0].keys()))
        writer.writeheader()
        writer.writerows(metrics_rows)
    print(f"metricas guardadas en {metrics_csv}")

    print("\n=== Tiempo, speedup y eficiencia (promedio +/- IC95%) ===")
    for row in metrics_rows:
        ci_w = (row["ci95_high"] - row["ci95_low"]) / 2
        if row["engine"] == "sequential":
            print(f"  {row['engine']:<15} tiempo={row['mean_time_sec']:.6f}s +/- {ci_w:.6f}  (n={row['n']})")
        else:
            sp_w = (row["speedup_ci95_high"] - row["speedup_ci95_low"]) / 2
            print(f"  {row['engine']:<15} threads={row['threads']:<2} tiempo={row['mean_time_sec']:.6f}s +/- {ci_w:.6f}  "
                  f"speedup={row['speedup']:.3f} +/- {sp_w:.3f}  eficiencia={row['efficiency']*100:.1f}%  (n={row['n']})")

    print("\n=== Ley de Amdahl: fs ajustado por minimos cuadrados (solo hilos <= nucleos fisicos) ===")
    amdahl_rows = []
    for engine, pairs in speedups_by_engine.items():
        used = fit_points(pairs, cores)
        if len(used) < 2:  # ej. smt/smt_ctrl (un solo n): con 1 punto el ajuste de fs no significa nada
            print(f"  {engine}: menos de 2 valores de hilos utilizables -> sin ajuste de Amdahl")
            continue
        fs = fit_fs(used)
        if fs is None:
            continue
        print(f"  {engine}: fs (fraccion paralelizable) ajustado = {fs:.4f}  (con n = {', '.join(str(n) for n, _ in sorted(used))})")
        for n, measured in sorted(pairs):
            predicted = amdahl_speedup(fs, n)
            in_fit = (n, measured) in used
            region = "fisico" if (cores is None or n <= cores) else "smt"
            amdahl_rows.append({"engine": engine, "threads": n, "region": region, "en_ajuste": "si" if in_fit else "no",
                                "speedup_medido": measured, "speedup_amdahl_ajustado": predicted})
            note = "" if in_fit else "  (fuera del ajuste)"
            print(f"    threads={n:<2} [{region}] medido={measured:.3f}  amdahl_ajustado={predicted:.3f}{note}")

    if amdahl_rows:
        amdahl_csv = out_prefix.with_name(out_prefix.name + "_amdahl.csv")
        with amdahl_csv.open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(amdahl_rows[0].keys()))
            writer.writeheader()
            writer.writerows(amdahl_rows)
        print(f"comparacion con Amdahl guardada en {amdahl_csv}")

    # --- SMT contra su control: mismo binario y mismos hilos, en CPUs hermanas
    # (smt) o en nucleos fisicos distintos (smt_ctrl). La diferencia aisla el
    # efecto de compartir nucleo sin tocar la BIOS.
    smt_cfg = next((k for k in groups if k[0] == "smt"), None)
    ctrl_cfg = next((k for k in groups if k[0] == "smt_ctrl"), None)
    if smt_cfg and ctrl_cfg and smt_cfg[1] == ctrl_cfg[1]:
        n_thr = smt_cfg[1]
        s_mean, _, s_sem, _ = mean_ci95(groups[smt_cfg])
        c_mean, _, c_sem, _ = mean_ci95(groups[ctrl_cfg])
        ratio, ratio_ci = speedup_ci(s_mean, s_sem, c_mean, c_sem)  # tiempo_smt / tiempo_ctrl
        sp_smt, sp_ctrl = seq_mean / s_mean, seq_mean / c_mean
        print(f"\n=== SMT contra su control ({n_thr} hilos) ===")
        print(f"  smt      (CPUs hermanas, mismo nucleo):   {s_mean:.6f}s  speedup={sp_smt:.3f}")
        print(f"  smt_ctrl (nucleos fisicos distintos):     {c_mean:.6f}s  speedup={sp_ctrl:.3f}")
        signif = "diferencia significativa (el IC no incluye 1)" if (ratio_ci[0] > 1 or ratio_ci[1] < 1) else "diferencia NO significativa (el IC incluye 1)"
        print(f"  smt tarda {100 * (ratio - 1):.1f}% mas que el control: tiempo_smt/tiempo_ctrl = {ratio:.3f}, "
              f"IC95% [{ratio_ci[0]:.3f}, {ratio_ci[1]:.3f}] -> {signif}")
        print(f"  el hilo extra en el mismo nucleo aporta {100 * (sp_smt - 1):.0f}% sobre 1 hilo; en otro nucleo, {100 * (sp_ctrl - 1):.0f}%")
        smt_csv = out_prefix.with_name(out_prefix.name + "_smt.csv")
        with smt_csv.open("w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["threads", "tiempo_smt", "tiempo_ctrl", "ratio_smt_ctrl", "ratio_ci95_low", "ratio_ci95_high", "speedup_smt", "speedup_ctrl"])
            writer.writerow([n_thr, s_mean, c_mean, ratio, ratio_ci[0], ratio_ci[1], sp_smt, sp_ctrl])
        print(f"comparacion smt vs control guardada en {smt_csv}")

    print("\n=== Convergencia (promedio acumulado vs. promedio final; revisa diferencia_pct) ===")
    conv_rows = []
    for (engine, threads), values in sorted(groups.items()):
        for c, running, pct in convergence(values):
            conv_rows.append({"engine": engine, "threads": threads, "corridas": c,
                               "promedio_acumulado": running, "diferencia_pct": pct})
    if conv_rows:
        conv_csv = out_prefix.with_name(out_prefix.name + "_convergencia.csv")
        with conv_csv.open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(conv_rows[0].keys()))
            writer.writeheader()
            writer.writerows(conv_rows)
        print(f"datos de convergencia guardados en {conv_csv}")

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("\n(matplotlib no esta instalado - me salto las graficas; 'pip3 install matplotlib' para tenerlas)", file=sys.stderr)
        return

    # 1) boxplot de tiempos por configuracion
    labels, data = [], []
    for (engine, threads), values in sorted(groups.items()):
        labels.append(engine if engine == "sequential" else f"{engine}\nn={threads}")
        data.append(values)
    fig, ax = plt.subplots(figsize=(max(8, len(labels) * 0.9), 5))
    ax.boxplot(data, showfliers=False)  # labels por separado: boxplot() cambio el nombre del parametro entre versiones de matplotlib
    ax.set_xticks(range(1, len(labels) + 1))
    ax.set_xticklabels(labels)
    ax.set_ylabel("tiempo (s)")
    ax.set_title("Distribucion de tiempos por configuracion")
    plt.xticks(rotation=45, ha="right")
    fig.tight_layout()
    boxplot_path = out_prefix.with_name(out_prefix.name + "_boxplot.png")
    fig.savefig(boxplot_path, dpi=150)
    plt.close(fig)
    print(f"boxplot guardado en {boxplot_path}")

    # 2) speedup medido vs amdahl ajustado vs lineal ideal, por motor
    if speedups_by_engine:
        from matplotlib.lines import Line2D
        all_threads = sorted({n for pairs in speedups_by_engine.values() for n, _ in pairs})
        x_max = max(all_threads)
        INK, MUTED = "#0b0b0b", "#52514e"
        # Paleta categorica validada (daltonismo): azul, naranja, aqua, violeta + gris.
        # Cada motor lleva ademas su propio marcador, asi no depende solo del color.
        style = {
            "cmp":           dict(color="#2a78d6", marker="o", ms=9,  lw=2.6, zorder=6, label="cmp (un hilo por nucleo)"),
            "smt_ctrl":      dict(color="#1baf7a", marker="P", ms=11, lw=0,   zorder=5, label="smt_ctrl (2 hilos, nucleos distintos)"),
            "smt":           dict(color="#eb6834", marker="D", ms=9,  lw=0,   zorder=5, label="smt (2 hilos, mismo nucleo)"),
            "coarseGrained": dict(color="#4a3aa7", marker="s", ms=9,  lw=1.4, ls="--", mfc="white", mew=1.6, zorder=4, label="coarseGrained"),
            "fineGrained":   dict(color="#52514e", marker="^", ms=8,  lw=1.4, zorder=3, label="fineGrained"),
        }
        # coarse y fine miden ~1.0x en todos los n: se separan un poco en x para
        # que ambos se vean; smt y smt_ctrl comparten n=2 con cmp, igual.
        dx = {"coarseGrained": -0.09, "fineGrained": 0.09, "smt": 0.14, "smt_ctrl": -0.14}

        fig, ax = plt.subplots(figsize=(9, 6))
        fig.patch.set_facecolor("white")

        # zona SMT (mas hilos que nucleos fisicos), detras de todo
        if cores:
            ax.axvspan(cores + 0.001, x_max + 0.6, color="#efeeea", zorder=0)
            ax.axvline(cores, color="#bdbcb6", lw=1, zorder=1)

        # referencia: speedup lineal ideal (se corta en el limite del eje y)
        y_top = max(5.5, max(s for pairs in speedups_by_engine.values() for _, s in pairs) * 1.45)
        ax.plot([0.5, x_max], [0.5, x_max], linestyle="-.", color="#9a9893", lw=1.3, zorder=2)

        ax.axhline(1.0, color="#9a9893", lw=0.8, zorder=1)

        for engine in ("fineGrained", "coarseGrained", "smt_ctrl", "smt", "cmp"):
            pairs = speedups_by_engine.get(engine)
            if not pairs:
                continue
            st = dict(style[engine]); label = st.pop("label")
            pairs_sorted = sorted(pairs)
            xs = [p[0] + dx.get(engine, 0.0) for p in pairs_sorted]
            ys = [p[1] for p in pairs_sorted]
            ax.plot(xs, ys, **st)

            # curva de Amdahl solo donde aporta (fs ~ 0 es la recta y=1, ruido)
            used = fit_points(pairs, cores)
            fs = fit_fs(used) if len(used) >= 2 else None
            if fs is not None and fs > 0.05:
                nx = [p[0] for p in pairs_sorted]
                ax.plot(nx, [amdahl_speedup(fs, n) for n in nx], linestyle=":", lw=1.8,
                        color=style[engine]["color"], alpha=0.85, zorder=4)
                ax.annotate(f"Amdahl ajustado (fs={fs:.2f})", (nx[-1], amdahl_speedup(fs, nx[-1])),
                            xytext=(-2, 12), textcoords="offset points", ha="right",
                            fontsize=9, color=MUTED)

        # etiquetas directas (valor medido) solo donde cuentan
        def tag(engine, n, text, xy_off, ha="left"):
            for (nn, s) in speedups_by_engine.get(engine, []):
                if nn == n:
                    ax.annotate(text.format(s), (n + dx.get(engine, 0.0), s), xytext=xy_off,
                                textcoords="offset points", ha=ha, va="center",
                                fontsize=9.5, color=INK, zorder=7)
        for n in (2, 4, 8):
            tag("cmp", n, "{:.2f}x", (-12, 4) if n != 2 else (12, -5), ha="right" if n != 2 else "left")
        tag("smt_ctrl", 2, "smt_ctrl {:.2f}x", (-12, 12), ha="right")
        tag("smt", 2, "smt {:.2f}x", (12, -6), ha="left")
        ax.annotate("coarseGrained y fineGrained\n~1.0x: no hay paralelismo real", (6.0, 1.0),
                    xytext=(0, 14), textcoords="offset points", ha="center",
                    fontsize=9, color=MUTED)

        if cores:
            ax.text(cores + 0.1, y_top - 0.08,
                    f"mas hilos que nucleos fisicos ({cores}): SMT\n(fuera del ajuste de Amdahl)",
                    fontsize=9, color=MUTED, va="top", ha="left", zorder=3)
            ax.text(cores - 0.1, y_top - 0.08, f"{cores} nucleos fisicos", fontsize=9, color=MUTED,
                    va="top", ha="right", zorder=3)

        ax.set_xlim(0.5, x_max + 0.6)
        ax.set_ylim(0.5, y_top)
        ax.set_xticks(all_threads)
        ax.set_xlabel("cantidad de hilos", fontsize=11, color=INK)
        ax.set_ylabel("speedup (t secuencial / t del motor)", fontsize=11, color=INK)
        ax.set_title("Escalabilidad: speedup medido vs. lineal ideal", fontsize=13, color=INK, loc="left", pad=12)
        ax.grid(axis="y", color="#e6e5e0", lw=0.8, zorder=0)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        for side in ("left", "bottom"):
            ax.spines[side].set_color("#9a9893")
        ax.tick_params(colors=MUTED, labelsize=10)

        # leyenda fuera del area de datos, una sola fila de marcadores por motor
        handles = []
        for engine in ("cmp", "smt_ctrl", "smt", "coarseGrained", "fineGrained"):
            if engine in speedups_by_engine:
                st = style[engine]
                handles.append(Line2D([], [], color=st["color"], marker=st["marker"], ms=8,
                                      lw=st.get("lw", 0) and 1.6, ls=st.get("ls", "-"),
                                      mfc=st.get("mfc", st["color"]), mew=st.get("mew", 1.0),
                                      label=st["label"]))
        handles.append(Line2D([], [], color="#9a9893", ls="-.", lw=1.3, label="speedup lineal ideal (llega a 8x)"))
        handles.append(Line2D([], [], color=style["cmp"]["color"], ls=":", lw=1.8, label="Amdahl ajustado (cmp)"))
        ax.legend(handles=handles, loc="upper center", bbox_to_anchor=(0.5, -0.12), ncol=3,
                  fontsize=9.5, frameon=False, labelcolor=INK, columnspacing=1.6)

        fig.tight_layout()
        scal_path = out_prefix.with_name(out_prefix.name + "_escalabilidad.png")
        fig.savefig(scal_path, dpi=170, facecolor="white", bbox_inches="tight")
        plt.close(fig)
        print(f"grafica de escalabilidad guardada en {scal_path}")


if __name__ == "__main__":
    main()
