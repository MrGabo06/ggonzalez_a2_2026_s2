#!/usr/bin/env python3
"""
analyze_campaign.py

Post-processing for the N-body measurement campaign. Reads the four final
CSV files (one per execution model), computes per-configuration statistics
(mean, 95% confidence interval via Student's t), derives speedup and
efficiency against the sequential baseline, and produces the plots the
project specification asks for: boxplots per N and a convergence plot of
the confidence interval width vs. number of repetitions.

Usage: python3 analyze_campaign.py
Expects final_cmp.csv, final_smt.csv, final_cgmt.csv, final_fgmt.csv in the
current directory. Writes summary.csv and PNG plots alongside them.
"""

import pandas as pd
import numpy as np
from scipy import stats
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

COLUMNS = ["model", "n", "steps", "workers", "time_sec", "checksum", "ns_per_interaction"]

FILES = {
    "cmp": "final_cmp.csv",
    "smt": "final_smt.csv",
    "cgmt": "final_cgmt.csv",
    "fgmt": "final_fgmt.csv",
}


def LoadAllRuns():
    """
    Purpose: Reads the four campaign CSV files and concatenates them into
             one DataFrame, keeping only the columns common to all of them
             (cgmt/fgmt have an extra chunk/quantum column we don't need
             here, since it was fixed to a single value in the final run).
    Returns: A single DataFrame with one row per individual run.
    """
    frames = []
    for label, path in FILES.items():
        df = pd.read_csv(path)
        frames.append(df[COLUMNS])
    return pd.concat(frames, ignore_index=True)


def SummarizeByConfig(runs):
    """
    Purpose: Groups runs by (model, n, workers) and computes the mean,
             standard deviation, sample count, and 95% confidence interval
             half-width (Student's t, since sample sizes are finite) of
             ns_per_interaction for each configuration.
    """
    groups = runs.groupby(["model", "n", "workers"])["ns_per_interaction"]
    summary = groups.agg(mean="mean", std="std", count="count").reset_index()

    # 95% CI half-width = t_(0.975, df) * std / sqrt(count)
    tValues = stats.t.ppf(0.975, summary["count"] - 1)
    summary["ci95_halfwidth"] = tValues * summary["std"] / np.sqrt(summary["count"])

    return summary


def AddSpeedupAndEfficiency(summary):
    """
    Purpose: Adds speedup (T_sequential / T_model) and efficiency
             (speedup / workers) columns, using the sequential row for the
             same N as the baseline.
    """
    baseline = (
        summary[summary["model"] == "sequential"]
        .set_index("n")["mean"]
        .to_dict()
    )

    summary["baseline_mean"] = summary["n"].map(baseline)
    summary["speedup"] = summary["baseline_mean"] / summary["mean"]
    summary["efficiency"] = summary["speedup"] / summary["workers"]

    return summary.drop(columns=["baseline_mean"])


def PlotBoxplotsByN(runs):
    """
    Purpose: One boxplot figure per N, comparing ns_per_interaction across
             models and worker counts. Visualizes spread, not just the mean.
    """
    for n in sorted(runs["n"].unique()):
        subset = runs[runs["n"] == n]
        subset = subset[subset["model"] != "sequential"]

        labels = []
        data = []
        for (model, workers), group in subset.groupby(["model", "workers"]):
            labels.append(f"{model}\n{workers}w")
            data.append(group["ns_per_interaction"].values)

        fig, ax = plt.subplots(figsize=(max(8, len(labels) * 0.6), 5))
        ax.boxplot(data, tick_labels=labels, showfliers=False)
        ax.set_ylabel("ns por interacción")
        ax.set_title(f"Distribución de tiempos por configuración (N={n})")
        plt.xticks(rotation=90)
        plt.tight_layout()
        plt.savefig(f"boxplot_n{n}.png", dpi=150)
        plt.close(fig)


def PlotConvergence(runs, model, n, workers):
    """
    Purpose: Shows how the 95% CI half-width shrinks as more repetitions
             are included, for one representative configuration, to justify
             that 200 repetitions is enough for the sample mean to stabilize.
    """
    subset = runs[
        (runs["model"] == model) & (runs["n"] == n) & (runs["workers"] == workers)
    ]["ns_per_interaction"].values

    if len(subset) < 10:
        print(f"Advertencia: muy pocas muestras para {model} N={n} workers={workers}, se omite.")
        return

    halfWidths = []
    sampleCounts = range(5, len(subset) + 1)
    for count in sampleCounts:
        sample = subset[:count]
        tValue = stats.t.ppf(0.975, count - 1)
        halfWidth = tValue * np.std(sample, ddof=1) / np.sqrt(count)
        halfWidths.append(halfWidth)

    fig, ax = plt.subplots(figsize=(7, 4))
    ax.plot(list(sampleCounts), halfWidths, marker="o", markersize=2)
    ax.set_xlabel("Número de repeticiones usadas")
    ax.set_ylabel("Ancho del IC 95% (ns/interacción)")
    ax.set_title(f"Convergencia del IC 95% ({model}, N={n}, workers={workers})")
    plt.tight_layout()
    plt.savefig(f"convergence_{model}_n{n}_w{workers}.png", dpi=150)
    plt.close(fig)


def main():
    runs = LoadAllRuns()

    summary = SummarizeByConfig(runs)
    summary = AddSpeedupAndEfficiency(summary)
    summary.to_csv("summary.csv", index=False)
    print(f"Resumen guardado en summary.csv ({len(summary)} configuraciones).")

    PlotBoxplotsByN(runs)
    print("Boxplots guardados (boxplot_n<N>.png).")

    for model in ["cmp", "smt", "cgmt", "fgmt"]:
        PlotConvergence(runs, model, n=4096, workers=16)
    print("Gráficas de convergencia guardadas (convergence_<modelo>_n4096_w16.png).")


if __name__ == "__main__":
    main()
