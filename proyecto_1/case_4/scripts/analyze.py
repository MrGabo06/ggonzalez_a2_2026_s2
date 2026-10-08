#!/usr/bin/env python3
"""analyze.py - calcula speedup, eficiencia, escalabilidad, intervalo de
confianza 95%, convergencia, y compara el speedup medido contra la
prediccion teorica de la Ley de Amdahl, a partir del CSV que genera
bench.py.

fs (fraccion paralelizable) se estima por minimos cuadrados a partir de
los propios datos medidos, no a ojo del codigo: para cada cantidad de
hilos n>1 se despeja fs de la ecuacion de Amdahl S(n) = 1/(fs + (1-fs)/n),
y se ajusta por minimos cuadrados (recta forzada por el origen) usando
todos los n>1 disponibles para ese motor. Es mas objetivo y facil de
justificar en la defensa que estimarlo viendo el codigo.

Uso:
    python3 scripts/analyze.py results/bench_oficial.csv
    python3 scripts/analyze.py results/bench_oficial.csv --out results/analisis
"""
import argparse
import csv
import math
import statistics
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
    args = ap.parse_args()

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

    print("\n=== Ley de Amdahl: fs ajustado por minimos cuadrados a los datos medidos ===")
    amdahl_rows = []
    for engine, pairs in speedups_by_engine.items():
        if len(pairs) < 2:  # ej. smt/smt_ctrl (un solo n): con 1 punto el ajuste de fs no significa nada
            print(f"  {engine}: un solo valor de hilos -> sin ajuste de Amdahl")
            continue
        fs = fit_fs(pairs)
        if fs is None:
            continue
        print(f"  {engine}: fs (fraccion paralelizable) ajustado = {fs:.4f}")
        for n, measured in sorted(pairs):
            ideal = amdahl_speedup(fs, n)
            amdahl_rows.append({"engine": engine, "threads": n, "speedup_medido": measured, "speedup_amdahl_ideal": ideal})
            print(f"    threads={n:<2} medido={measured:.3f}  amdahl_ideal={ideal:.3f}")

    if amdahl_rows:
        amdahl_csv = out_prefix.with_name(out_prefix.name + "_amdahl.csv")
        with amdahl_csv.open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(amdahl_rows[0].keys()))
            writer.writeheader()
            writer.writerows(amdahl_rows)
        print(f"comparacion con Amdahl guardada en {amdahl_csv}")

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

    # 2) speedup medido vs amdahl ideal vs lineal, por motor
    if speedups_by_engine:
        all_threads = sorted({n for pairs in speedups_by_engine.values() for n, _ in pairs})
        fig, ax = plt.subplots(figsize=(7, 5))
        for engine, pairs in speedups_by_engine.items():
            pairs_sorted = sorted(pairs)
            xs = [p[0] for p in pairs_sorted]
            ys = [p[1] for p in pairs_sorted]
            ax.plot(xs, ys, marker="o", label=f"{engine} (medido)")
            fs = fit_fs(pairs) if len(pairs) >= 2 else None
            if fs is not None:
                ax.plot(xs, [amdahl_speedup(fs, n) for n in xs], linestyle="--",
                        label=f"{engine} (Amdahl ideal, fs={fs:.2f})")
        ax.plot(all_threads, all_threads, linestyle=":", color="gray", label="speedup lineal ideal")
        ax.set_xlabel("cantidad de hilos")
        ax.set_ylabel("speedup")
        ax.set_title("Escalabilidad: speedup medido vs. ideal (Amdahl y lineal)")
        ax.legend(fontsize=8)
        fig.tight_layout()
        scal_path = out_prefix.with_name(out_prefix.name + "_escalabilidad.png")
        fig.savefig(scal_path, dpi=150)
        plt.close(fig)
        print(f"grafica de escalabilidad guardada en {scal_path}")


if __name__ == "__main__":
    main()
