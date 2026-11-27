#!/usr/bin/env python3
"""Puts the FPGA next to the CPU and GPU results: tables and figures.

Usage (from proyecto_2):
    python3 scripts/compare_platforms.py

Inputs:
    results/*.csv           one CSV per CPU/GPU machine (make report)
    results/fpga/*.csv      fpga rows (make fpga_jtag_bench, merged across files) and the CPU baseline
                            measured on the host that drives the board (make fpga_baseline), same sizes

Outputs in results/figuras/:
    comparativa_tabla.md        FPGA against its own host, and every platform at a common size
    comparativa_tiempo.png      time against image size, host CPU and FPGA
    comparativa_speedup.png     speedup against the host seq, per FPGA size

Speedup = median of seq / median of the platform, same filter and size. For the FPGA two times
are reported: kernel (hardware cycle counter / 50 MHz) and total (includes the JTAG transfers).
A speedup below 1 means slower than seq.
"""
import csv
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent
FILTERS = ["gauss", "sobel"]
SIMD_BACKENDS = ["simd", "simd_sep", "simd512", "simd_sep512"]
CUDA_BACKENDS = ["cuda", "cuda_tiled"]


def machine_name(path):
    return re.sub(r"_\d{8}_\d{4}$", "", path.stem)


def load(path):
    """Returns {(backend, filter, side): {"median": ms, "kernel": ms or None}}."""
    data = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            data[(r["backend"], r["filter"], int(r["width"]))] = {
                "median": float(r["median_ms"]),
                "kernel": float(r["kernel_median_ms"]) if r.get("kernel_median_ms") else None,
            }
    return data


def is_fpga(backend):
    return backend.startswith("fpga")


def split_fpga_dir(directory):
    """FPGA rows are merged from every file that has them (the newest file wins for the same
    filter and size, so a short sweep at another size adds to the others); the rest is the host baseline."""
    fpga, baseline, baseline_name = {}, {}, None
    for path in sorted(directory.glob("*.csv")):
        data = load(path)
        if any(is_fpga(k[0]) for k in data):
            fpga.update({k: v for k, v in data.items() if is_fpga(k[0])})
        else:
            baseline, baseline_name = data, machine_name(path).replace("_baseline", "")
    return fpga, baseline, baseline_name


def best(data, backends, flt, size, field="median"):
    """(name, value) of the fastest backend among `backends`, or None."""
    options = [(b, data[(b, flt, size)][field]) for b in backends
               if (b, flt, size) in data and data[(b, flt, size)][field] is not None]
    return min(options, key=lambda o: o[1]) if options else None


def ratio(num, den):
    if not num or not den:
        return "-"
    value = num / den
    return f"{value:.1f}x" if value >= 10 else (f"{value:.2f}x" if value >= 0.1 else f"{value:.3f}x")


def fmt_ms(value):
    if value is None:
        return "-"
    return f"{value:.4f}" if value < 0.1 else (f"{value:.3f}" if value < 100 else f"{value:.1f}")


def host_table(fpga, base, host):
    sizes = sorted({k[2] for k in fpga})
    fpga_names = sorted({k[0] for k in fpga if is_fpga(k[0])})
    lines = [f"## FPGA contra la CPU que la maneja ({host})", "",
             "Mismas imágenes y mismos filtros. La CPU se midió en la máquina conectada a la "
             "tarjeta, con repeticiones suficientes para los tamaños pequeños.", ""]
    for flt in FILTERS:
        lines += [f"### {flt}: mediana en ms", "", "| | " + " | ".join(f"{s}²" for s in sizes) + " |",
                  "|---|" + "---|" * len(sizes)]
        seq = [base.get(("seq", flt, s), {}).get("median") for s in sizes]
        simd = [best(base, SIMD_BACKENDS, flt, s) for s in sizes]
        lines.append("| seq | " + " | ".join(fmt_ms(v) for v in seq) + " |")
        lines.append("| mejor SIMD | " + " | ".join(f"{fmt_ms(o[1])} ({o[0]})" if o else "-" for o in simd) + " |")
        for name in fpga_names:
            kernel = [fpga.get((name, flt, s), {}).get("kernel") for s in sizes]
            total = [fpga.get((name, flt, s), {}).get("median") for s in sizes]
            lines.append(f"| {name} kernel | " + " | ".join(fmt_ms(v) for v in kernel) + " |")
            lines.append(f"| {name} total (con JTAG) | " + " | ".join(fmt_ms(v) for v in total) + " |")
            lines.append(f"| {name} kernel / total | " + " | ".join(
                f"{k / t * 100:.1f} %" if k and t else "-" for k, t in zip(kernel, total)) + " |")
            lines.append(f"| speedup del kernel contra seq | " + " | ".join(ratio(s, k) for s, k in zip(seq, kernel)) + " |")
            lines.append(f"| speedup del total contra seq | " + " | ".join(ratio(s, t) for s, t in zip(seq, total)) + " |")
        lines.append("| speedup del mejor SIMD contra seq | " + " | ".join(
            ratio(s, o[1]) if o else "-" for s, o in zip(seq, simd)) + " |")
        lines.append("")
    return lines


def all_platforms_table(machines, fpga, base, host, size):
    lines = [f"## Todas las plataformas en {size}² (el tamaño común)", "",
             "El speedup de cada fila es contra el `seq` de **su propia máquina**. Las columnas de "
             "CUDA con copias incluyen el viaje de la imagen a la GPU y de vuelta; el kernel no.", "",
             "| Plataforma | Dato | gauss (ms) | gauss speedup | sobel (ms) | sobel speedup |",
             "|---|---|---|---|---|---|"]

    def row(platform, label, values, seqs):
        cells = []
        for v, s in zip(values, seqs):
            cells += [fmt_ms(v), ratio(s, v)]
        lines.append(f"| {platform} | {label} | " + " | ".join(cells) + " |")

    def entries(data, platform):
        seqs = [data.get(("seq", f, size), {}).get("median") for f in FILTERS]
        if not all(seqs):
            return
        row(platform, "seq", seqs, seqs)
        for label, group, field in (("mejor SIMD", SIMD_BACKENDS, "median"),
                                    ("mejor CUDA con copias", CUDA_BACKENDS, "median"),
                                    ("mejor CUDA solo kernel", CUDA_BACKENDS, "kernel")):
            picks = [best(data, group, f, size, field) for f in FILTERS]
            if all(picks):
                row(platform, f"{label} ({picks[0][0]})", [p[1] for p in picks], seqs)

    for name, data in machines.items():
        entries(data, name)
    if base:
        entries(base, f"{host} (CPU de la FPGA)")
    seqs = [base.get(("seq", f, size), {}).get("median") for f in FILTERS] if base else [None, None]
    for backend in sorted({k[0] for k in fpga if is_fpga(k[0])}):
        kernel = [fpga.get((backend, f, size), {}).get("kernel") for f in FILTERS]
        total = [fpga.get((backend, f, size), {}).get("median") for f in FILTERS]
        if all(kernel):
            row(f"FPGA {backend}", "kernel (ciclos / 50 MHz)", kernel, seqs)
            row(f"FPGA {backend}", "total con JTAG", total, seqs)
    lines.append("")
    return lines


def plot_time(fpga, base, host, out):
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    sizes = sorted({k[2] for k in fpga})
    names = sorted({k[0] for k in fpga if is_fpga(k[0])})
    for ax, flt in zip(axes, FILTERS):
        series = {"seq (CPU)": [base.get(("seq", flt, s), {}).get("median") for s in sizes],
                  "mejor SIMD (CPU)": [(best(base, SIMD_BACKENDS, flt, s) or (0, None))[1] for s in sizes]}
        for name in names:
            series[f"{name} kernel"] = [fpga.get((name, flt, s), {}).get("kernel") for s in sizes]
            series[f"{name} total (JTAG)"] = [fpga.get((name, flt, s), {}).get("median") for s in sizes]
        for label, ys in series.items():
            pts = [(s, y) for s, y in zip(sizes, ys) if y]
            if pts:
                ax.plot(*zip(*pts), marker="o", label=label,
                        linestyle="--" if "total" in label else "-")
        ax.set(xscale="log", yscale="log", xlabel="lado de la imagen (px)", ylabel="mediana (ms)",
               title=f"Filtro {flt}")
        ax.set_xticks(sizes)
        ax.set_xticklabels([str(s) for s in sizes])
        ax.minorticks_off()
        ax.grid(True, which="both", alpha=0.3)
    axes[1].legend(fontsize=8)
    fig.suptitle(f"Tiempo contra tamaño: CPU de {host} y FPGA")
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def plot_speedup(fpga, base, host, out):
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    sizes = sorted({k[2] for k in fpga})
    names = sorted({k[0] for k in fpga if is_fpga(k[0])})
    for ax, flt in zip(axes, FILTERS):
        seq = [base.get(("seq", flt, s), {}).get("median") for s in sizes]
        series = {"mejor SIMD": [(best(base, SIMD_BACKENDS, flt, s) or (0, None))[1] for s in sizes]}
        for name in names:
            series[f"{name} kernel"] = [fpga.get((name, flt, s), {}).get("kernel") for s in sizes]
            series[f"{name} total (JTAG)"] = [fpga.get((name, flt, s), {}).get("median") for s in sizes]
        for label, ys in series.items():
            pts = [(s, q / y) for s, q, y in zip(sizes, seq, ys) if q and y]
            if pts:
                ax.plot(*zip(*pts), marker="o", label=label,
                        linestyle="--" if "total" in label else "-")
        ax.axhline(1, color="gray", linestyle=":", linewidth=1)
        ax.set(xscale="log", yscale="log", xlabel="lado de la imagen (px)",
               ylabel="speedup contra seq (x)", title=f"Filtro {flt}")
        ax.set_xticks(sizes)
        ax.set_xticklabels([str(s) for s in sizes])
        ax.minorticks_off()
        ax.grid(True, which="both", alpha=0.3)
    axes[1].legend(fontsize=8)
    fig.suptitle(f"Speedup contra el seq de {host} (menor a 1 = más lento que seq)")
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def main():
    fpga, base, host = split_fpga_dir(ROOT / "results" / "fpga")
    if not fpga:
        sys.exit("No hay filas del backend fpga en results/fpga/. Corra make fpga_jtag_bench")
    if not base:
        sys.exit("Falta la linea base de CPU de la maquina de la FPGA. Corra make fpga_baseline")
    machines = {machine_name(p): load(p) for p in sorted((ROOT / "results").glob("*.csv"))}

    common = sorted({k[2] for k in fpga} & {k[2] for d in machines.values() for k in d})
    lines = [f"# Comparativa de plataformas: CPU, GPU y FPGA", ""]
    lines += host_table(fpga, base, host)
    for size in common:
        lines += all_platforms_table(machines, fpga, base, host, size)

    outdir = ROOT / "results" / "figuras"
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / "comparativa_tabla.md").write_text("\n".join(lines), encoding="utf-8")
    plot_time(fpga, base, host, outdir / "comparativa_tiempo.png")
    plot_speedup(fpga, base, host, outdir / "comparativa_speedup.png")
    print(f"Escrito en {outdir}/comparativa_* (tamanos comunes con CPU y GPU: {common or 'ninguno'})")


if __name__ == "__main__":
    main()
