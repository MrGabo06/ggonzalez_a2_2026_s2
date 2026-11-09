#!/usr/bin/env python3
"""Tablas y gráficas de F4 a partir de los CSV que genera scripts/run_bench.sh.

Uso (desde proyecto_2):
    python3 scripts/analyze.py                    # todos los CSV de results/
    python3 scripts/analyze.py results/mi.csv     # solo ese archivo

Por cada CSV (una máquina) escribe en results/figuras/:
    <maquina>_tabla.md        speedup, eficiencia y error numérico
    <maquina>_tiempo.png      mediana (ms) contra tamaño
    <maquina>_speedup.png     speedup contra seq, por tamaño
    <maquina>_eficiencia.png  eficiencia de SIMD (speedup / ancho del vector)
    <maquina>_kernel.png      tiempo solo del kernel de CUDA (si el CSV trae kernel_median_ms)

Definiciones:
  speedup    = mediana de seq / mediana del backend (mismo filtro y tamaño)
  eficiencia = speedup de un backend SIMD sobre SU versión secuencial (simd contra seq,
               simd_sep contra seq_sep) dividido entre las pasadas que procesa por
               instrucción: 8 floats en AVX2 y 16 en AVX-512. 100 % sería un
               aprovechamiento perfecto del vector. En CUDA no aplica (no hay un ancho único).
  escalabilidad = cómo cambian tiempo y speedup al crecer la imagen (se ve en las gráficas)
  kernel     = tiempo del kernel en la GPU medido con cudaEvent, sin copiar la imagen entre
               CPU y GPU. La mediana normal de CUDA sí incluye esas copias (extremo a extremo).
"""
import csv
import sys
from collections import OrderedDict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # sin ventana: solo escribe PNG
import matplotlib.pyplot as plt
from matplotlib.ticker import NullFormatter

# Backend SIMD -> (backend secuencial equivalente, píxeles por instrucción)
SIMD_INFO = {
    "simd": ("seq", 8),
    "simd_sep": ("seq_sep", 8),
    "simd512": ("seq", 16),
    "simd_sep512": ("seq_sep", 16),
}
ORDER = ["seq", "seq_sep", "simd", "simd_sep", "simd512", "simd_sep512", "cuda", "cuda_tiled"]
FILTERS = ["gauss", "sobel"]


def load(path):
    """Devuelve {(backend, filtro, lado): fila} con números ya convertidos."""
    data = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            key = (r["backend"], r["filter"], int(r["width"]))
            data[key] = {
                "median": float(r["median_ms"]),
                "stddev": float(r["stddev_ms"]),
                "err": int(r["max_abs_error"]) if r["max_abs_error"] != "" else None,
                "ok": r["ok"],
                # Solo los backends de GPU la traen (CSV viejos no tienen la columna)
                "kernel": float(r["kernel_median_ms"]) if r.get("kernel_median_ms") else None,
            }
    return data


def backends_in(data):
    names = {k[0] for k in data}
    return [b for b in ORDER if b in names] + sorted(names - set(ORDER))


def sizes_in(data):
    return sorted({k[2] for k in data})


def speedup(data, backend, flt, size, base="seq"):
    a, b = data.get((base, flt, size)), data.get((backend, flt, size))
    return a["median"] / b["median"] if a and b else None


def write_table(data, out):
    backends, sizes = backends_in(data), sizes_in(data)
    lines = []
    for flt in FILTERS:
        lines += [f"### {flt}: mediana en ms (speedup contra seq)", ""]
        lines += ["| Backend | " + " | ".join(f"{s}²" for s in sizes) + " |"]
        lines += ["|---|" + "---|" * len(sizes)]
        for b in backends:
            cells = []
            for s in sizes:
                row = data.get((b, flt, s))
                sp = speedup(data, b, flt, s)
                cells.append(f"{row['median']:.3f} ({sp:.1f}x)" if row and sp else "-")
            lines.append(f"| {b} | " + " | ".join(cells) + " |")
        lines.append("")
    lines += ["### Eficiencia de SIMD (speedup contra su secuencial / píxeles por instrucción)", "",
              "Un valor mayor a 100 % es posible: el baseline secuencial hace más trabajo por píxel "
              "que solo multiplicar (clamp en cada vecino, cálculo de índices), y el código SIMD lo "
              "evita en el interior de la imagen. Por eso la eficiencia mide el aprovechamiento "
              "del vector *más* esa diferencia de código, no solo el paralelismo.", ""]
    for flt in FILTERS:
        lines += [f"**{flt}**", "", "| Backend | " + " | ".join(f"{s}²" for s in sizes) + " |"]
        lines += ["|---|" + "---|" * len(sizes)]
        for b, (base, lanes) in SIMD_INFO.items():
            if b not in backends:
                continue
            cells = []
            for s in sizes:
                sp = speedup(data, b, flt, s, base)
                cells.append(f"{sp / lanes * 100:.0f} %" if sp else "-")
            lines.append(f"| {b} ({lanes} por instr.) | " + " | ".join(cells) + " |")
        lines.append("")
    lines += kernel_table(data, backends, sizes)
    errs = [v["err"] for v in data.values() if v["err"] is not None]
    bad = sorted({k[0] for k, v in data.items() if v["ok"] != "1"})
    lines += ["### Diferencias numéricas contra seq", "",
              f"Error máximo por píxel en todas las corridas: {max(errs) if errs else 'sin datos'}. "
              f"Backends con alguna corrida fuera de tolerancia: {', '.join(bad) if bad else 'ninguno'}.", ""]
    Path(out).write_text("\n".join(lines), encoding="utf-8")


GPU_BACKENDS = ["cuda", "cuda_tiled"]


def has_kernel_data(data):
    return any(v["kernel"] is not None for v in data.values())


def kernel_table(data, backends, sizes):
    """Tabla del kernel de CUDA: solo kernel, extremo a extremo y cuánto pesan las copias."""
    if not has_kernel_data(data):
        return []
    lines = ["### Kernel de CUDA sin copias de memoria", "",
             "El tiempo extremo a extremo incluye copiar la imagen a la GPU y traer el resultado; "
             "el del kernel es solo el cálculo en la GPU. 'Copias' es la parte del tiempo total "
             "que no es kernel.", ""]
    for flt in FILTERS:
        lines += [f"**{flt}**", "", "| Backend | Dato | " + " | ".join(f"{s}²" for s in sizes) + " |"]
        lines += ["|---|---|" + "---|" * len(sizes)]
        for b in [g for g in GPU_BACKENDS if g in backends]:
            rows = [data.get((b, flt, s)) for s in sizes]
            kern = [f"{r['kernel']:.3f} ms" if r and r["kernel"] is not None else "-" for r in rows]
            e2e = [f"{r['median']:.3f} ms" if r else "-" for r in rows]
            copy = [f"{(1 - r['kernel'] / r['median']) * 100:.0f} %" if r and r["kernel"] is not None else "-"
                    for r in rows]
            sp = [f"{data[('seq', flt, s)]['median'] / r['kernel']:.0f}x"
                  if r and r["kernel"] and ("seq", flt, s) in data else "-" for r, s in zip(rows, sizes)]
            lines.append(f"| {b} | solo kernel | " + " | ".join(kern) + " |")
            lines.append(f"| {b} | extremo a extremo | " + " | ".join(e2e) + " |")
            lines.append(f"| {b} | copias | " + " | ".join(copy) + " |")
            lines.append(f"| {b} | speedup del kernel contra seq | " + " | ".join(sp) + " |")
        if all(("cuda", flt, s) in data and ("cuda_tiled", flt, s) in data for s in sizes):
            ratio = []
            for s in sizes:
                g, t = data[("cuda", flt, s)]["kernel"], data[("cuda_tiled", flt, s)]["kernel"]
                ratio.append(f"{g / t:.2f}x" if g and t else "-")
            lines.append("| cuda_tiled contra cuda | kernel (mayor a 1 = tiled más rápido) | "
                         + " | ".join(ratio) + " |")
        lines.append("")
    return lines


def plot_kernel(data, machine, out):
    sizes = sizes_in(data)
    backends = [b for b in GPU_BACKENDS if any((b, f, s) in data for f in FILTERS for s in sizes)]
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    width = 0.8 / max(len(backends), 1)
    for ax, flt in zip(axes, FILTERS):
        for i, b in enumerate(backends):
            vals = [(data.get((b, flt, s)) or {}).get("kernel") or 0 for s in sizes]
            ax.bar([x + i * width for x in range(len(sizes))], vals, width, label=b)
        ax.set_xticks([x + 0.4 - width / 2 for x in range(len(sizes))])
        ax.set_xticklabels([f"{s}²" for s in sizes])
        ax.set(ylabel="kernel en la GPU (ms)", xlabel="tamaño", title=f"Filtro {flt}")
        ax.grid(True, axis="y", alpha=0.3)
    axes[1].legend(fontsize=8)
    fig.suptitle(f"Tiempo solo del kernel de CUDA, sin copias ({machine})")
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def plot_time(data, machine, out):
    backends, sizes = backends_in(data), sizes_in(data)
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharey=False)
    for ax, flt in zip(axes, FILTERS):
        for b in backends:
            ys = [data[(b, flt, s)]["median"] if (b, flt, s) in data else None for s in sizes]
            pts = [(s, y) for s, y in zip(sizes, ys) if y]
            if pts:
                ax.plot(*zip(*pts), marker="o", label=b)
        ax.set(xscale="log", yscale="log", xlabel="lado de la imagen (px)", ylabel="mediana (ms)",
               title=f"Filtro {flt}")
        ax.set_xticks(sizes)
        ax.set_xticklabels([str(s) for s in sizes])
        ax.xaxis.set_minor_formatter(NullFormatter())
        ax.grid(True, which="both", alpha=0.3)
    axes[1].legend(fontsize=8)
    fig.suptitle(f"Tiempo contra tamaño ({machine})")
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def plot_speedup(data, machine, out):
    backends = [b for b in backends_in(data) if b != "seq"]
    sizes = sizes_in(data)
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    width = 0.8 / len(sizes)
    for ax, flt in zip(axes, FILTERS):
        for i, s in enumerate(sizes):
            vals = [speedup(data, b, flt, s) or 0 for b in backends]
            ax.bar([x + i * width for x in range(len(backends))], vals, width, label=f"{s}²")
        ax.set_xticks([x + 0.4 - width / 2 for x in range(len(backends))])
        ax.set_xticklabels(backends, rotation=35, ha="right", fontsize=8)
        ax.set(ylabel="speedup contra seq (x)", title=f"Filtro {flt}")
        ax.grid(True, axis="y", alpha=0.3)
    axes[1].legend(title="tamaño", fontsize=8)
    fig.suptitle(f"Speedup contra el secuencial ({machine})")
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def plot_efficiency(data, machine, out):
    sizes = sizes_in(data)
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    for ax, flt in zip(axes, FILTERS):
        for b, (base, lanes) in SIMD_INFO.items():
            pts = [(s, speedup(data, b, flt, s, base)) for s in sizes]
            pts = [(s, sp / lanes * 100) for s, sp in pts if sp]
            if pts:
                ax.plot(*zip(*pts), marker="o", label=b)
        ax.axhline(100, color="gray", linestyle="--", linewidth=0.8)
        ax.set(xscale="log", xlabel="lado de la imagen (px)", ylabel="eficiencia (%)",
               title=f"Filtro {flt}")
        ax.set_xticks(sizes)
        ax.set_xticklabels([str(s) for s in sizes])
        ax.xaxis.set_minor_formatter(NullFormatter())
        ax.grid(True, alpha=0.3)
    axes[1].legend(fontsize=8)
    fig.suptitle(f"Eficiencia de SIMD ({machine})")
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def main():
    root = Path(__file__).resolve().parent.parent
    paths = [Path(p) for p in sys.argv[1:]] or sorted((root / "results").glob("*.csv"))
    if not paths:
        sys.exit("No hay CSV en results/. Corra primero scripts/run_bench.sh")
    outdir = root / "results" / "figuras"
    outdir.mkdir(parents=True, exist_ok=True)
    for path in paths:
        data = load(path)
        machine = path.stem
        if not data:
            print(f"{path.name}: sin filas, se omite")
            continue
        write_table(data, outdir / f"{machine}_tabla.md")
        plot_time(data, machine, outdir / f"{machine}_tiempo.png")
        plot_speedup(data, machine, outdir / f"{machine}_speedup.png")
        plot_efficiency(data, machine, outdir / f"{machine}_eficiencia.png")
        if has_kernel_data(data):
            plot_kernel(data, machine, outdir / f"{machine}_kernel.png")
        print(f"{path.name}: {len(data)} filas -> {outdir}/{machine}_*")


if __name__ == "__main__":
    main()
