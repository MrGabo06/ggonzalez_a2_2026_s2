#!/usr/bin/env python3
"""
plot_cpu_affinity.py

Convierte el muestreo de scripts/sample_cpu_affinity.sh (que TID corrio
en que CPU logica, cada ~10ms) en un mapa de ocupacion por CPU logica,
similar a la Fig. 2f del companero del caso 3. Cada muestra de un TID en
una CPU logica cuenta como "ocupado" ese instante; se normaliza contra
la CPU logica mas ocupada.

Uso: python3 scripts/plot_cpu_affinity.py <csv_on> <csv_off> <N>
Ejemplo: python3 scripts/plot_cpu_affinity.py \
    results/perf/cpu_affinity_on.csv results/perf/cpu_affinity_off.csv 4096
Escribe results/figures/cpu_affinity_n<N>.png
"""
import sys
import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

FIGURES_DIR = "results/figures"


def LoadCounts(path):
    """
    Purpose: Cuenta cuantas muestras cayeron en cada CPU logica (psr),
             como proxy de que tan ocupada estuvo esa CPU durante la
             corrida.
    """
    df = pd.read_csv(path)
    counts = df["psr"].value_counts().sort_index()
    allCpus = range(0, int(df["psr"].max()) + 1)
    counts = counts.reindex(allCpus, fill_value=0)
    return counts


def main():
    if len(sys.argv) < 4:
        print("Uso: python3 scripts/plot_cpu_affinity.py <csv_on> <csv_off> <N>")
        sys.exit(1)

    pathOn, pathOff, n = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(FIGURES_DIR, exist_ok=True)

    countsOn = LoadCounts(pathOn)
    countsOff = LoadCounts(pathOff)

    fig, (axOn, axOff) = plt.subplots(2, 1, figsize=(8, 5), sharex=True)

    for ax, counts, title in [(axOn, countsOn, "SMT encendido"),
                               (axOff, countsOff, "SMT apagado")]:
        pct = 100 * counts / counts.max() if counts.max() > 0 else counts
        ax.bar([f"cpu{i}" for i in counts.index], pct.values)
        ax.set_ylabel("% de la CPU\nmas ocupada")
        ax.set_title(f"Ocupacion por CPU logica -- {title} (N={n})")
        ax.set_ylim(0, 105)

    plt.xticks(rotation=45)
    plt.tight_layout()
    outPath = os.path.join(FIGURES_DIR, f"cpu_affinity_n{n}.png")
    plt.savefig(outPath, dpi=150)
    print(f"Guardado {outPath}")


if __name__ == "__main__":
    main()
