#!/usr/bin/env python3
"""
analyze.py — Post-procesamiento del Caso 1 (multiplicacion de matrices por
bloques) para el framework de modelos de ejecucion multithreading (CE4302).

Lee los CSV generados por `make sweep` (uno por modelo:
sequential/fine_grained/coarse_grained/smt/cmp, columnas
model,n,block_size,workers,time_sec,checksum) y produce:

  1. results/summary.csv
     Una fila por (modelo, workers): media, desviacion estandar, n de
     muestras, IC 95% del tiempo, speedup, eficiencia y la cantidad de
     checksums distintos observados (para detectar inconsistencias).

  2. figures/c1-speedup.pdf
     Speedup vs P (workers) para los 4 modelos paralelos, con barras de
     error = IC 95%, mas la linea de speedup ideal (S_p = p) como
     referencia.

  3. figures/c1-efficiency.pdf
     Eficiencia (S_p / p) vs P para los 4 modelos paralelos.

  4. figures/c1-boxplot-<model>.pdf
     Un boxplot por modelo paralelo: distribucion del tiempo de ejecucion
     por cada valor de P (exigido por el enunciado: graficas de
     distribucion, no solo promedios).

  5. figures/c1-convergencia.pdf
     Media acumulada (running mean) del tiempo de cmp en su P maximo,
     para discutir si ITER (200 por defecto) es suficiente para que la
     media se estabilice.

Uso:
    python3 analyze.py --results-dir results --out-dir .
    python3 analyze.py                      # usa los defaults de arriba

Requisitos: pandas, numpy, matplotlib (ya confirmados disponibles).
"""

import argparse
import math
import os
import sys

import numpy as np
import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

MODELS_PARALLEL = ["fine_grained", "coarse_grained", "smt", "cmp"]
MODEL_LABELS = {
    "fine_grained": "FGMT",
    "coarse_grained": "CGMT",
    "smt": "SMT",
    "cmp": "CMP",
    "sequential": "Secuencial",
}
Z_95 = 1.959963984540054  # z para 95% (normal estandar)


def ci95(series: pd.Series) -> float:
    """Semi-ancho del intervalo de confianza al 95%: 1.96 * s / sqrt(n).

    Con n=1 (una sola muestra para esa combinacion) no hay forma de
    estimar la dispersion; se devuelve NaN en vez de 0, para no sugerir
    falsamente una certeza perfecta en las graficas/tabla.
    """
    n = series.count()
    if n < 2:
        return float("nan")
    return Z_95 * series.std(ddof=1) / math.sqrt(n)


def load_model_csv(results_dir: str, filename: str, model_name: str) -> pd.DataFrame:
    path = os.path.join(results_dir, filename)
    if not os.path.isfile(path):
        print(f"  [!] No se encontro {path}; se omite el modelo '{model_name}'.")
        return pd.DataFrame(
            columns=["model", "n", "block_size", "workers", "time_sec", "checksum"]
        )
    df = pd.read_csv(path)
    expected_cols = {"model", "n", "block_size", "workers", "time_sec", "checksum"}
    missing = expected_cols - set(df.columns)
    if missing:
        sys.exit(f"ERROR: {path} no tiene las columnas esperadas (faltan: {missing})")
    # Por si el nombre de 'model' en el CSV no coincide exactamente con la
    # clave que usamos aqui (p.ej. 'fine' vs 'fine_grained'), lo forzamos.
    df["model"] = model_name
    return df


def load_all(results_dir: str) -> pd.DataFrame:
    frames = [
        load_model_csv(results_dir, "sequential.csv", "sequential"),
        load_model_csv(results_dir, "fine_grained.csv", "fine_grained"),
        load_model_csv(results_dir, "coarse_grained.csv", "coarse_grained"),
        load_model_csv(results_dir, "smt.csv", "smt"),
        load_model_csv(results_dir, "cmp.csv", "cmp"),
    ]
    df = pd.concat(frames, ignore_index=True)
    if df.empty:
        sys.exit("ERROR: no se cargo ningun dato. Corre 'make sweep' primero.")
    return df


def check_checksums(df: pd.DataFrame) -> None:
    """Verifica que, para cada (n, block_size, seed-implicita via checksum
    agrupado por modelo+workers), los distintos modelos concuerden en el
    resultado numerico. Como no guardamos la semilla en el CSV, se valida
    agregando: el CONJUNTO de checksums observados por cada modelo debe
    ser igual (como conjunto) al de 'sequential', que es la referencia.
    """
    print("\n=== Verificacion de checksums entre modelos ===")
    seq_checksums = set(df.loc[df["model"] == "sequential", "checksum"].round(3))
    if not seq_checksums:
        print("  [!] No hay datos de 'sequential'; no se puede usar como referencia.")
        return
    all_ok = True
    for model in MODELS_PARALLEL:
        sub = df.loc[df["model"] == model, "checksum"]
        if sub.empty:
            continue
        model_checksums = set(sub.round(3))
        extra = model_checksums - seq_checksums
        missing = seq_checksums - model_checksums
        if extra or missing:
            all_ok = False
            print(f"  [X] {MODEL_LABELS[model]}: checksums NO coinciden con sequential.")
            if extra:
                print(f"       valores en {model} que no estan en sequential: {sorted(extra)[:5]}...")
            if missing:
                print(f"       valores de sequential ausentes en {model}: {sorted(missing)[:5]}...")
        else:
            print(f"  [OK] {MODEL_LABELS[model]}: mismos checksums que sequential.")
    if all_ok:
        print("  -> Todos los modelos son numericamente consistentes.")
    else:
        print("  -> Hay inconsistencias: revisar antes de reportar resultados en el paper.")


def compute_summary(df: pd.DataFrame) -> pd.DataFrame:
    """Una fila por (model, workers): media/desv/n/IC95% del tiempo."""
    grouped = (
        df.groupby(["model", "workers"])["time_sec"]
        .agg(mean_time="mean", std_time="std", n="count")
        .reset_index()
    )
    ci = (
        df.groupby(["model", "workers"])["time_sec"]
        .apply(ci95)
        .reset_index(name="ci95_time")
    )
    grouped = grouped.merge(ci, on=["model", "workers"])

    n_checksums = (
        df.groupby(["model", "workers"])["checksum"]
        .apply(lambda s: s.round(3).nunique())
        .reset_index(name="n_checksums_distintos")
    )
    grouped = grouped.merge(n_checksums, on=["model", "workers"])

    # T1: tiempo medio secuencial de referencia (baseline), un solo valor.
    seq_row = grouped.loc[grouped["model"] == "sequential"]
    if seq_row.empty:
        sys.exit("ERROR: no hay fila 'sequential' para usar como T1 (baseline).")
    t1 = seq_row["mean_time"].iloc[0]

    grouped["speedup"] = t1 / grouped["mean_time"]
    grouped["efficiency"] = grouped["speedup"] / grouped["workers"]

    # IC95% propagado al speedup via derivada (delta method):
    # Sp = T1 / Tp  =>  sigma_Sp ~= Sp * (sigma_Tp / Tp)   (T1 fijo/conocido)
    grouped["ci95_speedup"] = grouped["speedup"] * (
        grouped["ci95_time"] / grouped["mean_time"]
    )

    grouped = grouped.sort_values(["model", "workers"]).reset_index(drop=True)
    return grouped


def plot_speedup(summary: pd.DataFrame, out_path: str) -> None:
    fig, ax = plt.subplots(figsize=(6, 4.2))
    max_p = 1
    for model in MODELS_PARALLEL:
        sub = summary[summary["model"] == model].sort_values("workers")
        if sub.empty:
            continue
        max_p = max(max_p, sub["workers"].max())
        ax.errorbar(
            sub["workers"],
            sub["speedup"],
            yerr=sub["ci95_speedup"].fillna(0),
            marker="o",
            capsize=3,
            label=MODEL_LABELS[model],
        )
    ideal_p = np.arange(1, max_p + 1)
    ax.plot(ideal_p, ideal_p, "k--", linewidth=1, label="Speedup ideal")
    ax.set_xlabel("Numero de hilos / cores (P)")
    ax.set_ylabel("Speedup $S_p = T_1 / T_p$")
    ax.set_title("Caso 1: Speedup vs P (multiplicacion de matrices por bloques)")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path)
    plt.close(fig)
    print(f"  -> {out_path}")


def plot_efficiency(summary: pd.DataFrame, out_path: str) -> None:
    fig, ax = plt.subplots(figsize=(6, 4.2))
    for model in MODELS_PARALLEL:
        sub = summary[summary["model"] == model].sort_values("workers")
        if sub.empty:
            continue
        ax.errorbar(
            sub["workers"],
            sub["efficiency"],
            yerr=(sub["ci95_speedup"] / sub["workers"]).fillna(0),
            marker="o",
            capsize=3,
            label=MODEL_LABELS[model],
        )
    ax.axhline(1.0, color="k", linestyle="--", linewidth=1, label="Eficiencia ideal")
    ax.set_xlabel("Numero de hilos / cores (P)")
    ax.set_ylabel("Eficiencia $E_p = S_p / p$")
    ax.set_title("Caso 1: Eficiencia paralela vs P")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path)
    plt.close(fig)
    print(f"  -> {out_path}")


def plot_boxplots(df: pd.DataFrame, figures_dir: str) -> None:
    for model in MODELS_PARALLEL:
        sub = df[df["model"] == model]
        if sub.empty:
            continue
        workers_sorted = sorted(sub["workers"].unique())
        data = [sub.loc[sub["workers"] == w, "time_sec"].values for w in workers_sorted]

        fig, ax = plt.subplots(figsize=(6, 4.2))
        ax.boxplot(data, tick_labels=[str(w) for w in workers_sorted])
        ax.set_xlabel("Numero de hilos / cores (P)")
        ax.set_ylabel("Tiempo de ejecucion (s)")
        ax.set_title(f"Caso 1: Distribucion del tiempo — {MODEL_LABELS[model]}")
        ax.grid(True, axis="y", alpha=0.3)
        fig.tight_layout()
        out_path = os.path.join(figures_dir, f"c1-boxplot-{model}.pdf")
        fig.savefig(out_path)
        plt.close(fig)
        print(f"  -> {out_path}")


def plot_convergence(df: pd.DataFrame, out_path: str, model: str = "cmp") -> None:
    sub = df[df["model"] == model]
    if sub.empty:
        print(f"  [!] No hay datos de '{model}' para la grafica de convergencia; se omite.")
        return
    max_p = sub["workers"].max()
    series = sub.loc[sub["workers"] == max_p, "time_sec"].reset_index(drop=True)
    running_mean = series.expanding().mean()

    fig, ax = plt.subplots(figsize=(6, 4.2))
    ax.plot(range(1, len(running_mean) + 1), running_mean, marker=".", markersize=3)
    ax.axhline(running_mean.iloc[-1], color="gray", linestyle="--", linewidth=1)
    ax.set_xlabel("Numero de repeticiones acumuladas")
    ax.set_ylabel("Media acumulada del tiempo (s)")
    ax.set_title(
        f"Caso 1: Convergencia de la media — {MODEL_LABELS[model]} (P={max_p})"
    )
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path)
    plt.close(fig)
    print(f"  -> {out_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results-dir", default="results", help="Carpeta con los CSV de entrada (default: results)")
    parser.add_argument("--out-dir", default=".", help="Carpeta base de salida; se crean out-dir/results y out-dir/figures (default: .)")
    args = parser.parse_args()

    figures_dir = os.path.join(args.out_dir, "figures")
    summary_dir = os.path.join(args.out_dir, "results")
    os.makedirs(figures_dir, exist_ok=True)
    os.makedirs(summary_dir, exist_ok=True)

    print(f"Leyendo CSVs desde '{args.results_dir}'...")
    df = load_all(args.results_dir)
    print(f"  {len(df)} filas cargadas en total.")

    check_checksums(df)

    print("\n=== Calculando resumen (media, IC95%, speedup, eficiencia) ===")
    summary = compute_summary(df)
    summary_path = os.path.join(summary_dir, "summary.csv")
    summary.to_csv(summary_path, index=False)
    print(f"  -> {summary_path}")
    print("\n" + summary.to_string(index=False))

    print("\n=== Generando figuras ===")
    plot_speedup(summary, os.path.join(figures_dir, "c1-speedup.pdf"))
    plot_efficiency(summary, os.path.join(figures_dir, "c1-efficiency.pdf"))
    plot_boxplots(df, figures_dir)
    plot_convergence(df, os.path.join(figures_dir, "c1-convergencia.pdf"), model="cmp")

    print("\nListo.")


if __name__ == "__main__":
    main()