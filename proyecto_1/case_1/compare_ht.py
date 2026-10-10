#!/usr/bin/env python3
"""
compare_ht.py -- Comparacion de resultados con Hyper-Threading APAGADO vs ENCENDIDO
(Caso 1: multiplicacion de matrices por bloques, CE4302).

Entrada : dos summary.csv generados por analyze.py (uno por condicion de HT).
Salida  : - <out-dir>/ht_comparison.csv      (tabla completa)
          - <out-dir>/c1-ht-speedup.pdf      (speedup HT off vs HT on, SMT y CMP)
          - <out-dir>/c1-ht-tiempos.pdf      (tiempo medio absoluto, todos los modelos)
          - tabla resumen por consola

Nota metodologica: el IC95% del speedup aqui SI incluye la incertidumbre del
baseline secuencial (de la propia condicion), propagada en cuadratura:
    ci(S) = S * sqrt( (ci_Tp/Tp)^2 + (ci_T1/T1)^2 )
analyze.py solo propaga la incertidumbre de T_p, asi que los IC de este script
son un poco mas anchos (sobre todo cuando el baseline es ruidoso).

Uso:
    python3 compare_ht.py --off out_ht_off_fix/results/summary.csv \
                          --on  out_ht_on_sib/results/summary.csv \
                          --out-dir comparison

Tambien sirve para comparar cualquier par de corridas con otras etiquetas, p. ej.
pinning "cores" vs "siblings" con HT encendido:
    python3 compare_ht.py --off out_ht_on_fix/results/summary.csv --label-off "HT on, pin cores" \
                          --on  out_ht_on_sib/results/summary.csv --label-on  "HT on, pin siblings" \
                          --out-dir comparison_pin
"""
import argparse
import os
import sys

import numpy as np
import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

MODELS = ["fine_grained", "coarse_grained", "smt", "cmp"]
LABELS = {"fine_grained": "FGMT", "coarse_grained": "CGMT", "smt": "SMT", "cmp": "CMP"}
# Estilo: color + marcador + trazo, para que se distinga tambien en blanco y negro.
STYLE = {
    "off": dict(color="#1f77b4", marker="o", linestyle="-", label="HT apagado"),
    "on": dict(color="#d95f02", marker="s", linestyle="--", label="HT encendido"),
}


def load(path: str) -> pd.DataFrame:
    if not os.path.isfile(path):
        sys.exit(f"ERROR: no existe {path}")
    df = pd.read_csv(path)
    need = {"model", "workers", "mean_time", "ci95_time"}
    if not need.issubset(df.columns):
        sys.exit(f"ERROR: {path} no parece un summary.csv de analyze.py")
    base = df[df["model"] == "sequential"]
    if base.empty:
        sys.exit(f"ERROR: {path} no tiene la fila 'sequential'")
    t1 = float(base["mean_time"].iloc[0])
    c1 = float(base["ci95_time"].iloc[0])
    df = df[df["model"].isin(MODELS)].copy()
    df["speedup"] = t1 / df["mean_time"]
    rel = np.sqrt((df["ci95_time"] / df["mean_time"]) ** 2 + (c1 / t1) ** 2)
    df["ci95_speedup"] = df["speedup"] * rel
    df["efficiency"] = df["speedup"] / df["workers"]
    df.attrs["t1"] = t1
    df.attrs["c1"] = c1
    return df


def build_table(off: pd.DataFrame, on: pd.DataFrame) -> pd.DataFrame:
    cols = ["model", "workers", "mean_time", "ci95_time", "speedup", "ci95_speedup", "efficiency"]
    m = off[cols].merge(on[cols], on=["model", "workers"], suffixes=("_off", "_on"))
    m["delta_speedup"] = m["speedup_on"] - m["speedup_off"]
    m["time_ratio_on_off"] = m["mean_time_on"] / m["mean_time_off"]
    comb = np.sqrt(m["ci95_speedup_off"] ** 2 + m["ci95_speedup_on"] ** 2)
    m["diferencia_significativa"] = m["delta_speedup"].abs() > comb
    m["model"] = pd.Categorical(m["model"], MODELS, ordered=True)
    return m.sort_values(["model", "workers"]).reset_index(drop=True)


def plot_speedup(off, on, path):
    fig, axes = plt.subplots(1, 2, figsize=(10, 4.2), sharey=True)
    for ax, model in zip(axes, ["smt", "cmp"]):
        for tag, df in (("off", off), ("on", on)):
            d = df[df["model"] == model].sort_values("workers")
            st = STYLE[tag]
            ax.errorbar(d["workers"], d["speedup"], yerr=d["ci95_speedup"],
                        capsize=3, markersize=6, linewidth=1.6, **st)
        pmax = max(off[off["model"] == model]["workers"].max(),
                   on[on["model"] == model]["workers"].max())
        ax.plot([1, 4], [1, 4], color="gray", linewidth=1, linestyle=":", label="Ideal (hasta 4 nucleos)")
        ax.axhline(4, color="gray", linewidth=0.6, linestyle="-", alpha=0.4)
        ax.set_title(f"{LABELS[model]}: speedup")
        ax.set_xlabel("Numero de hilos / cores (P)")
        ax.set_xticks(sorted(set(off[off['model'] == model]['workers']) | set(on[on['model'] == model]['workers'])))
        ax.set_xlim(0.5, pmax + 0.5)
        ax.grid(alpha=0.3)
    axes[0].set_ylabel(r"Speedup $S_p = T_1/T_p$")
    axes[0].set_ylim(0, 4.6)
    axes[1].legend(loc="lower right", fontsize=9)
    fig.suptitle(f"Caso 1: {STYLE['off']['label']} vs {STYLE['on']['label']} (IC95%, baseline propio de cada condicion)")
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)


def plot_times(off, on, t_off, t_on, path):
    fig, axes = plt.subplots(1, 4, figsize=(13, 3.8), sharey=True)
    for ax, model in zip(axes, MODELS):
        for tag, df in (("off", off), ("on", on)):
            d = df[df["model"] == model].sort_values("workers")
            st = STYLE[tag]
            ax.errorbar(d["workers"], d["mean_time"], yerr=d["ci95_time"],
                        capsize=3, markersize=5, linewidth=1.4, **st)
        ax.axhline(t_off, color=STYLE["off"]["color"], linewidth=0.8, linestyle=":", alpha=0.7)
        ax.axhline(t_on, color=STYLE["on"]["color"], linewidth=0.8, linestyle=":", alpha=0.7)
        ax.set_title(LABELS[model])
        ax.set_xlabel("P")
        ax.set_xscale("log", base=2)
        ax.set_xticks(sorted(set(off[off['model'] == model]['workers'])))
        ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
        ax.grid(alpha=0.3)
    axes[0].set_ylabel("Tiempo medio (s)")
    axes[0].legend(fontsize=8)
    fig.suptitle("Tiempo absoluto por modelo (lineas punteadas: baseline secuencial de cada condicion)")
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser(description="Compara summary.csv con HT apagado vs encendido")
    ap.add_argument("--off", required=True, help="summary.csv con Hyper-Threading APAGADO")
    ap.add_argument("--on", required=True, help="summary.csv con Hyper-Threading ENCENDIDO")
    ap.add_argument("--out-dir", default="comparison")
    ap.add_argument("--label-off", default="HT apagado", help="etiqueta de la serie --off")
    ap.add_argument("--label-on", default="HT encendido", help="etiqueta de la serie --on")
    args = ap.parse_args()
    STYLE["off"]["label"] = args.label_off
    STYLE["on"]["label"] = args.label_on

    os.makedirs(args.out_dir, exist_ok=True)
    off, on = load(args.off), load(args.on)
    tab = build_table(off, on)

    csv_path = os.path.join(args.out_dir, "ht_comparison.csv")
    tab.to_csv(csv_path, index=False)

    print(f"Baseline secuencial: {args.label_off} = {off.attrs['t1']:.4f}s (IC95 +-{off.attrs['c1']:.4f}), "
          f"{args.label_on} = {on.attrs['t1']:.4f}s (IC95 +-{on.attrs['c1']:.4f}) "
          f"-> la segunda es {100 * (on.attrs['t1'] / off.attrs['t1'] - 1):+.1f}% en tiempo\n")

    show = tab.copy()
    show["S_off"] = show.apply(lambda r: f"{r.speedup_off:.2f}+-{r.ci95_speedup_off:.2f}", axis=1)
    show["S_on"] = show.apply(lambda r: f"{r.speedup_on:.2f}+-{r.ci95_speedup_on:.2f}", axis=1)
    show["dS"] = show["delta_speedup"].map(lambda v: f"{v:+.2f}")
    show["t_on/t_off"] = show["time_ratio_on_off"].map(lambda v: f"{v:.2f}")
    show["signif."] = show["diferencia_significativa"].map({True: "si", False: "no"})
    print(show[["model", "workers", "S_off", "S_on", "dS", "t_on/t_off", "signif."]].to_string(index=False))

    plot_speedup(off, on, os.path.join(args.out_dir, "c1-ht-speedup.pdf"))
    plot_times(off, on, off.attrs["t1"], on.attrs["t1"], os.path.join(args.out_dir, "c1-ht-tiempos.pdf"))
    print(f"\n-> {csv_path}\n-> {args.out_dir}/c1-ht-speedup.pdf\n-> {args.out_dir}/c1-ht-tiempos.pdf")


if __name__ == "__main__":
    import matplotlib.ticker  # noqa: F401  (usado en plot_times)
    main()