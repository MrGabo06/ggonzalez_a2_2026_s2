#!/usr/bin/env python3
"""
compare_smt_sessions.py

Compara CMP y SMT entre las dos sesiones de BIOS/SMT (encendido vs.
apagado) para un N fijo. Es la comparacion que el enunciado pide
especificamente para SMT (seccion 4). No modifica nada de lo que ya
genera analyze_campaign.py; lee los CSVs *_on.csv / *_off.csv aparte.

Uso: python3 scripts/compare_smt_sessions.py [N]
Si no se da N, usa 4096. Corre desde la raiz de case_2.
Espera results/raw/final_cmp_on.csv, final_cmp_off.csv,
final_smt_on.csv, final_smt_off.csv (cada uno con su propia fila
"sequential" para workers=1, que sirve de baseline de esa sesion).
Escribe results/raw/summary_smt_on_off.csv y
results/figures/smt_on_off_n<N>.png.
"""
import sys
import os
import pandas as pd
import numpy as np
from scipy import stats
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RAW_DIR = "results/raw"
FIGURES_DIR = "results/figures"
COLUMNS = ["model", "n", "steps", "workers", "time_sec", "checksum", "ns_per_interaction"]

N = int(sys.argv[1]) if len(sys.argv) > 1 else 4096


def LoadSession(label, suffix):
    """
    Purpose: Reads final_cmp_<suffix>.csv y final_smt_<suffix>.csv,
             filtra por N, y les marca de que sesion vienen.
    """
    frames = []
    for model in ["cmp", "smt"]:
        path = os.path.join(RAW_DIR, f"final_{model}_{suffix}.csv")
        df = pd.read_csv(path)[COLUMNS]
        frames.append(df)
    runs = pd.concat(frames, ignore_index=True)
    runs = runs[runs["n"] == N].copy()
    runs["session"] = label
    return runs


def Summarize(runs):
    """
    Purpose: Media, desviacion y semiancho del IC 95% por
             (sesion, modelo, workers), igual que analyze_campaign.py
             pero separando las dos sesiones de SMT.
    """
    groups = runs.groupby(["session", "model", "n", "workers"])["ns_per_interaction"]
    summary = groups.agg(mean="mean", std="std", count="count").reset_index()
    tValues = stats.t.ppf(0.975, summary["count"] - 1)
    summary["ci95_halfwidth"] = tValues * summary["std"] / np.sqrt(summary["count"])
    return summary


def AddSpeedup(summary, baselineOn, baselineOff):
    """
    Purpose: Speedup y eficiencia usando el baseline secuencial propio
             de cada sesion (no se mezclan sesiones distintas).
    """
    baselines = {"SMT on": baselineOn, "SMT off": baselineOff}
    summary["baseline_mean"] = summary["session"].map(baselines)
    summary["speedup"] = summary["baseline_mean"] / summary["mean"]
    summary["efficiency"] = summary["speedup"] / summary["workers"]
    return summary.drop(columns=["baseline_mean"])


def main():
    os.makedirs(RAW_DIR, exist_ok=True)
    os.makedirs(FIGURES_DIR, exist_ok=True)

    runsOn = LoadSession("SMT on", "on")
    runsOff = LoadSession("SMT off", "off")
    runs = pd.concat([runsOn, runsOff], ignore_index=True)

    seqOn = runsOn[runsOn["model"] == "sequential"]["ns_per_interaction"].mean()
    seqOff = runsOff[runsOff["model"] == "sequential"]["ns_per_interaction"].mean()
    print(f"Secuencial SMT on:  {seqOn:.4f} ns/interaccion")
    print(f"Secuencial SMT off: {seqOff:.4f} ns/interaccion "
          f"({100 * (seqOff - seqOn) / seqOn:+.2f}% vs. SMT on)")

    summary = Summarize(runs)
    summary = AddSpeedup(summary, seqOn, seqOff)
    summary = summary[summary["model"] != "sequential"]

    outPath = os.path.join(RAW_DIR, "summary_smt_on_off.csv")
    summary.to_csv(outPath, index=False)
    print(f"\nGuardado {outPath}\n")
    print(summary.to_string(index=False))

    fig, (axTime, axSpeedup) = plt.subplots(1, 2, figsize=(11, 4.5))
    markers = {"SMT on": "o", "SMT off": "s"}
    for model in ["cmp", "smt"]:
        for session in ["SMT on", "SMT off"]:
            subset = summary[(summary["model"] == model) & (summary["session"] == session)]
            subset = subset.sort_values("workers")
            if subset.empty:
                continue
            label = f"{model} ({session})"
            axTime.errorbar(subset["workers"], subset["mean"], yerr=subset["ci95_halfwidth"],
                             marker=markers[session], label=label, capsize=3)
            axSpeedup.plot(subset["workers"], subset["speedup"], marker=markers[session], label=label)

    axTime.set_xlabel("Hilos")
    axTime.set_ylabel("ns por interaccion")
    axTime.set_title(f"Tiempo medio, SMT on vs off (N={N})")
    axTime.legend(fontsize=8)

    axSpeedup.set_xlabel("Hilos")
    axSpeedup.set_ylabel("Speedup")
    axSpeedup.set_title(f"Speedup, SMT on vs off (N={N})")
    axSpeedup.legend(fontsize=8)

    plt.tight_layout()
    figPath = os.path.join(FIGURES_DIR, f"smt_on_off_n{N}.png")
    plt.savefig(figPath, dpi=150)
    print(f"\nGuardado {figPath}")


if __name__ == "__main__":
    main()
