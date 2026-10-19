#!/usr/bin/env python3
"""
sweep_quick_summary.py

Summarizes the defense-mode run (scripts/sweep_quick.sh): mean ns per
interaction with its 95% confidence interval (Student's t), speedup and
efficiency against the sequential baseline, and two checks:

  1. Correctness: every run of every model must end with the same checksum
     (same physics from the same seed). If not, the script exits with an
     error.
  2. Consistency with the full campaign: the ratio between the quick mean
     and the full-campaign mean (results/raw/final_*.csv, same N). It is
     informational: a ratio near 1.00 means the short run reproduces the
     long one.

Usage: python3 scripts/sweep_quick_summary.py [on|off]
Run from the case_2 root. The optional argument is the SMT label of the run
(default on). Reads results/quick/sweep_quick_<label>.csv and writes
results/quick/sweep_quick_summary_<label>.csv and
results/quick/sweep_quick_<label>.png. If the run of the other SMT state also
exists, it also prints an SMT on vs. off comparison.
"""

import os
import sys

import numpy as np
import pandas as pd
from scipy import stats
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

QUICK_DIR = "results/quick"
RAW_DIR = "results/raw"


def QuickCsv(label):
    return os.path.join(QUICK_DIR, f"sweep_quick_{label}.csv")


def SummaryCsv(label):
    return os.path.join(QUICK_DIR, f"sweep_quick_summary_{label}.csv")


def FigurePng(label):
    return os.path.join(QUICK_DIR, f"sweep_quick_{label}.png")

MODELS = ["cmp", "smt", "cgmt", "fgmt"]


def FullFiles(label):
    """Full-campaign files used as reference: same SMT session for CMP/SMT
    (CGMT and FGMT have a single full-campaign file)."""
    return [f"final_cmp_{label}.csv", f"final_smt_{label}.csv", "final_cgmt.csv", "final_fgmt.csv"]


def CheckChecksums(runs):
    """
    Purpose: Verifies that all runs share one checksum (rounded to 4
             decimals to ignore printing noise).
    Returns: The common checksum value; exits with error if they differ.
    """
    values = sorted(set(runs["checksum"].round(4)))
    if len(values) != 1:
        print(f"ERROR: los modelos NO coinciden, checksums distintos: {values}")
        sys.exit(1)
    print(f"Checksum identico en las {len(runs)} corridas: {values[0]} "
          f"(todos los modelos calculan lo mismo)")
    return values[0]


def Summarize(runs):
    """
    Purpose: Mean, standard deviation, count and 95% CI half-width of
             ns_per_interaction per (model, workers).
    """
    groups = runs.groupby(["model", "workers"])["ns_per_interaction"]
    summary = groups.agg(mean="mean", std="std", count="count").reset_index()
    tValues = stats.t.ppf(0.975, summary["count"] - 1)
    summary["ci95_halfwidth"] = tValues * summary["std"] / np.sqrt(summary["count"])
    return summary


def AddSpeedup(summary):
    """
    Purpose: Speedup = sequential mean / model mean (same N and steps, so
             the ratio of ns/interaction equals the ratio of times), and
             efficiency = speedup / workers.
    """
    seq = summary[summary["model"] == "sequential"]
    if seq.empty:
        print("ERROR: no hay fila 'sequential' (benchcmp con 1 hilo).")
        sys.exit(1)
    baseline = seq["mean"].iloc[0]
    summary["speedup"] = baseline / summary["mean"]
    summary["efficiency"] = summary["speedup"] / summary["workers"]
    return summary


def AddFullCampaignRatio(summary, n, label):
    """
    Purpose: Adds the column 'vs_completa' = quick mean / full-campaign mean
             for the same (model, workers) at the same N and SMT session.
    """
    frames = []
    for name in FullFiles(label):
        path = os.path.join(RAW_DIR, name)
        if os.path.exists(path):
            frames.append(pd.read_csv(path))
    if not frames:
        summary["vs_completa"] = np.nan
        return summary
    full = pd.concat(frames, ignore_index=True)
    full = full[full["n"] == n]
    fullMeans = full.groupby(["model", "workers"])["ns_per_interaction"].mean()
    summary["vs_completa"] = [
        row["mean"] / fullMeans[(row["model"], row["workers"])]
        if (row["model"], row["workers"]) in fullMeans.index else np.nan
        for _, row in summary.iterrows()
    ]
    return summary


def Plot(summary, n, label):
    """
    Purpose: Speedup and efficiency vs workers for each model, with the
             ideal line, in one PNG (same layout as the paper figure).
    """
    seq = summary[summary["model"] == "sequential"]
    fig, (axSpeedup, axEff) = plt.subplots(1, 2, figsize=(11, 4.5))
    maxWorkers = int(summary["workers"].max())
    axSpeedup.plot([1, maxWorkers], [1, maxWorkers], "k--", linewidth=1, label="speedup ideal")
    axEff.axhline(1.0, color="k", linestyle="--", linewidth=1, label="eficiencia ideal")
    for model in MODELS:
        data = summary[summary["model"] == model].sort_values("workers")
        if model == "cmp" and not seq.empty:
            data = pd.concat([seq, data]).sort_values("workers")
        if data.empty:
            continue
        axSpeedup.plot(data["workers"], data["speedup"], marker="o", label=model)
        axEff.plot(data["workers"], data["efficiency"], marker="o", label=model)
    axSpeedup.set_xlabel("Hilos / fibers")
    axSpeedup.set_ylabel("Speedup")
    axSpeedup.set_title(f"Speedup vs. trabajadores (N={n}, modo defensa, SMT {label})")
    axSpeedup.legend(fontsize=8)
    axEff.set_xlabel("Hilos / fibers")
    axEff.set_ylabel("Eficiencia")
    axEff.set_title(f"Eficiencia vs. trabajadores (N={n}, modo defensa, SMT {label})")
    axEff.legend(fontsize=8)
    plt.tight_layout()
    plt.savefig(FigurePng(label), dpi=150)


def CompareSessions(summaryLabel, label, otherLabel):
    """
    Purpose: Side-by-side SMT on vs. off comparison from the two quick runs.
             Each session uses its own sequential baseline for speedup.
    """
    other = pd.read_csv(QuickCsv(otherLabel))
    CheckChecksums(other)
    otherSummary = AddSpeedup(Summarize(other))
    ours = summaryLabel
    onSummary, offSummary = (ours, otherSummary) if label == "on" else (otherSummary, ours)
    merged = onSummary.merge(offSummary, on=["model", "workers"], suffixes=("_on", "_off"))
    merged["tiempo_off/on"] = merged["mean_off"] / merged["mean_on"]
    show = merged[["model", "workers", "mean_on", "mean_off", "tiempo_off/on",
                   "speedup_on", "speedup_off"]].sort_values(["model", "workers"])
    print("\n=== SMT encendido vs apagado (modo defensa) ===")
    print("tiempo_off/on > 1: con SMT apagado tarda mas; < 1: tarda menos\n")
    print(show.to_string(index=False, float_format=lambda x: f"{x:.3f}"))


def main():
    label = sys.argv[1] if len(sys.argv) > 1 else "on"
    if label not in ("on", "off"):
        print("El argumento debe ser 'on' u 'off'.")
        sys.exit(1)
    otherLabel = "off" if label == "on" else "on"

    if not os.path.exists(QuickCsv(label)):
        print(f"No existe {QuickCsv(label)}: corre scripts/sweep_quick.sh primero.")
        sys.exit(1)
    runs = pd.read_csv(QuickCsv(label))
    n = int(runs["n"].iloc[0])
    steps = int(runs["steps"].iloc[0])

    CheckChecksums(runs)

    summary = Summarize(runs)
    summary = AddSpeedup(summary)
    summary = AddFullCampaignRatio(summary, n, label)
    summary = summary.sort_values(["model", "workers"])
    summary.to_csv(SummaryCsv(label), index=False)
    Plot(summary, n, label)

    reps = int(summary["count"].min())
    print(f"\nSMT {label}: N={n}, pasos={steps}, repeticiones por configuracion: {reps}")
    print("Media = ns por interaccion; IC95 = semiancho del intervalo; "
          "vs_completa = media rapida / media de la campana completa de la misma sesion SMT\n")
    show = summary[["model", "workers", "mean", "ci95_halfwidth", "speedup",
                    "efficiency", "vs_completa"]]
    print(show.to_string(index=False, float_format=lambda x: f"{x:.3f}"))
    print(f"\nGuardado {SummaryCsv(label)} y {FigurePng(label)}")

    if os.path.exists(QuickCsv(otherLabel)):
        CompareSessions(summary, label, otherLabel)


if __name__ == "__main__":
    main()
