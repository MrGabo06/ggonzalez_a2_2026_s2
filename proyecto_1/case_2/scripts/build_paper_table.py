#!/usr/bin/env python3
"""
build_paper_table.py

Arma la fila de la tabla de metricas principales del paper (Tp en ms con
IC 95%, speedup, eficiencia) para un N fijo, a partir de los CSV crudos
de las 4 campanas -- la misma unidad (Tp en ms) que usan las tablas de
tus companeros, distinta al "ns por interaccion" que usa summary.csv.
Imprime una tabla legible y las filas ya formateadas en LaTeX.

Uso: python3 scripts/build_paper_table.py [N]
Corre desde la raiz de case_2. Espera results/raw/final_cmp_on.csv,
final_smt_on.csv, final_cgmt.csv, final_fgmt.csv.
"""
import sys
import os
import pandas as pd
import numpy as np
from scipy import stats

RAW_DIR = "results/raw"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 4096

FILES = {
    "cmp": "final_cmp_on.csv",
    "smt": "final_smt_on.csv",
    "cgmt": "final_cgmt.csv",
    "fgmt": "final_fgmt.csv",
}


def Load(model):
    path = os.path.join(RAW_DIR, FILES[model])
    df = pd.read_csv(path)
    return df[df["n"] == N]


def SummarizeTime(df):
    grouped = df.groupby(["model", "workers"])["time_sec"]
    summary = grouped.agg(mean="mean", std="std", count="count").reset_index()
    tVal = stats.t.ppf(0.975, summary["count"] - 1)
    summary["ci95"] = tVal * summary["std"] / np.sqrt(summary["count"])
    summary["mean_ms"] = summary["mean"] * 1000
    summary["ci95_ms"] = summary["ci95"] * 1000
    return summary


def main():
    frames = [Load(model) for model in ["cmp", "smt", "cgmt", "fgmt"]]
    allRuns = pd.concat(frames, ignore_index=True)

    summary = SummarizeTime(allRuns)

    seqRows = summary[summary["model"] == "sequential"]
    if seqRows.empty:
        print("ADVERTENCIA: no se encontro fila 'sequential' -- revisa que "
              "los CSV incluyan workers=1 etiquetado como sequential.")
        sys.exit(1)
    seqMean = seqRows["mean"].mean()

    summary["speedup"] = seqMean / summary["mean"]
    summary["efficiency"] = summary["speedup"] / summary["workers"]
    summary = summary.drop_duplicates(subset=["model", "workers"])
    summary = summary.sort_values(["model", "workers"])

    print(f"N = {N}\n")
    print(summary[["model", "workers", "mean_ms", "ci95_ms", "speedup", "efficiency"]]
          .to_string(index=False, float_format=lambda x: f"{x:.4f}"))

    print("\n--- Filas LaTeX (Tp en ms, pegar en la tabla) ---")
    for _, row in summary.iterrows():
        if row["model"] == "sequential":
            label = "Secuencial"
        else:
            label = f'{row["model"].upper()} ({int(row["workers"])})'
        print(f'{label} & ${row["mean_ms"]:.2f} \\pm {row["ci95_ms"]:.2f}$ & '
              f'${row["speedup"]:.2f}$ & ${row["efficiency"]:.2f}$ & COMPLETAR \\\\')


if __name__ == "__main__":
    main()
