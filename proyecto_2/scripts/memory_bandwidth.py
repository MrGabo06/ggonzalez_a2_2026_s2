#!/usr/bin/env python3
"""Memory bandwidth of the CPU, the GPU and the FPGA, derived from what was measured.

Usage (from proyecto_2):
    python3 scripts/memory_bandwidth.py

Reads the benchmark CSVs (results/*.csv, results/fpga/*.csv) and the Nsight Compute reports
(results/perfilado/<machine>_ncu/) and writes results/figuras/ancho_de_banda.md and .png.

Compulsory traffic: the image is read once and the output written once, one byte per pixel, so
T = 2 x N^2 bytes. The effective bandwidth is T / time. It is a lower bound of the real traffic,
not a measurement of the memory bus, except where the report says it comes from Nsight.
"""
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "fpga" / "scripts"))
from compare_platforms import ROOT, FILTERS, SIMD_BACKENDS, best, load, machine_name, split_fpga_dir  # noqa: E402
import pipeline_model  # noqa: E402

CUDA_BACKENDS = ["cuda", "cuda_tiled"]
LANES = 8
CLOCK_HZ = 50e6
DMEM_PEAK_BYTES_PER_CYCLE = LANES  # 8 banks of one byte, one access per cycle
JTAG_TCK_MHZ = 24  # fpga/jtag/vproc_jtag.sdc: USB-Blaster II, up to 24 MHz
DMEM_BYTES = 65536
RADIUS = {"gauss": 2, "sobel": 1}


def gbs(nbytes, ms):
    return nbytes / (ms * 1e-3) / 1e9


def cell(value, digits=2):
    return "-" if value is None else f"{value:.{digits}f}"


def table(headers, rows):
    lines = ["| " + " | ".join(headers) + " |", "|" + "---|" * len(headers)]
    return lines + ["| " + " | ".join(str(c) for c in r) + " |" for r in rows]


def cpu_section(machines, base, host):
    lines = ["## CPU: ancho de banda efectivo (GB/s)", "",
             "T / mediana, con T = 2 x N² bytes. Es un mínimo: no cuenta los accesos que resuelve la caché ni "
             "el relleno de bordes. En los tamaños que caben en caché no es tráfico de DRAM.", ""]
    sources = dict(machines)
    if base:
        sources[f"{host} (línea base de la FPGA)"] = base
    for name, data in sources.items():
        sizes = sorted({k[2] for k in data})
        rows = []
        for flt in FILTERS:
            for label, pick in (("seq", lambda s: ("seq", data.get(("seq", flt, s), {}).get("median"))),
                                ("mejor SIMD", lambda s: best(data, SIMD_BACKENDS, flt, s) or (None, None))):
                vals = [pick(s)[1] for s in sizes]
                rows.append([flt, label] + [cell(gbs(2 * s * s, v)) if v else "-" for s, v in zip(sizes, vals)])
        lines += [f"### {name}", ""] + table(["Filtro", "Backend"] + [f"{s}²" for s in sizes], rows) + [""]
    return lines


def parse_ncu(path):
    text = path.read_text(errors="replace")

    def grab(pattern):
        m = re.search(pattern, text)
        return float(m.group(1)) if m else None

    dur = re.search(r"Duration\s+(\w+)\s+([\d.]+)", text)
    scale = {"msecond": 1.0, "usecond": 1e-3, "nsecond": 1e-6, "second": 1e3}.get(dur.group(1), 1.0) if dur else 1.0
    return {"gbs": grab(r"Memory Throughput\s+Gbyte/second\s+([\d.]+)"), "dram_pct": grab(r"DRAM Throughput\s+%\s+([\d.]+)"),
            "ms": float(dur.group(2)) * scale if dur else None, "l1": grab(r"L1/TEX Hit Rate\s+%\s+([\d.]+)"),
            "l2": grab(r"L2 Hit Rate\s+%\s+([\d.]+)")}


def gpu_section(machines):
    lines = ["## GPU: kernel, copias y DRAM", "",
             "- **Kernel:** T / tiempo del kernel (cudaEvent).",
             "- **Copias (PCIe):** T / (extremo a extremo − kernel). Mueve N² bytes de ida y N² de vuelta.",
             "- **DRAM (Nsight):** el ancho de banda que mide Nsight Compute en la memoria del dispositivo, a 4096². "
             "El pico se deriva de Nsight como GB/s / (% de DRAM).", ""]
    series = {}
    for machine, data in machines.items():
        sizes = sorted({k[2] for k in data if k[0] in CUDA_BACKENDS})
        if not sizes:
            continue
        rows = []
        for backend in CUDA_BACKENDS:
            for flt in FILTERS:
                kern, copy = [], []
                for s in sizes:
                    r = data.get((backend, flt, s))
                    if r and r["kernel"]:
                        kern.append(cell(gbs(2 * s * s, r["kernel"]), 1))
                        copy.append(cell(gbs(2 * s * s, r["median"] - r["kernel"]), 1))
                        series.setdefault((machine, backend, flt, "kernel"), []).append((s, gbs(2 * s * s, r["kernel"])))
                        series.setdefault((machine, backend, flt, "e2e"), []).append((s, gbs(2 * s * s, r["median"])))
                    else:
                        kern.append("-")
                        copy.append("-")
                rows.append([backend, flt, "kernel"] + kern)
                rows.append([backend, flt, "copias (PCIe)"] + copy)
        lines += [f"### {machine}", ""] + table(["Backend", "Filtro", "Dato"] + [f"{s}²" for s in sizes], rows) + [""]
    lines += ["### DRAM medida por Nsight Compute a 4096²", ""]
    rows = []
    for machine in machines:
        for path in sorted((ROOT / "results" / "perfilado" / f"{machine}_ncu").glob("*.txt")):
            m = re.match(r"(cuda(?:_tiled)?)_(gauss|sobel)_(\d+)", path.stem)
            rep = parse_ncu(path)
            if not (m and rep["gbs"] and rep["ms"]):
                continue
            size = int(m.group(3))
            moved = rep["gbs"] * rep["ms"]
            peak = rep["gbs"] / (rep["dram_pct"] / 100) if rep["dram_pct"] else None
            rows.append([machine, m.group(1), m.group(2), f"{rep['gbs']:.1f}", f"{moved:.1f} MB",
                         f"{moved / (2 * size * size / 1e6):.2f}x", cell(peak, 0), f"{rep['dram_pct']:.1f} %",
                         f"{rep['l1']:.0f} %", f"{rep['l2']:.0f} %"])
    lines += table(["Máquina", "Backend", "Filtro", "DRAM (GB/s)", "Movido", "Movido / mínimo", "Pico (GB/s)", "% del pico",
                    "L1 hit", "L2 hit"], rows) + [""]
    return lines, series


def stream_counts(flt):
    """Bytes moved by VLD and VST per output pixel, from the executed instruction stream."""
    program = pipeline_model.assemble(flt, 64)
    stream = pipeline_model.run_stream(program)
    loads = sum(program[pc]["op"] == pipeline_model.VLD for pc in stream) * LANES
    stores = sum(program[pc]["op"] == pipeline_model.VST for pc in stream) * LANES
    return loads / 64 ** 2, stores / 64 ** 2


def jtag_bytes(flt, size):
    """Bytes sent and received per image, following the strip logic of the fpga backend."""
    r = RADIUS[flt]
    w8 = (size + LANES - 1) // LANES * LANES
    wp = w8 + 2 * r
    rows = min((DMEM_BYTES - wp * 2 * r) // (wp + w8), size)
    strips = (size + rows - 1) // rows
    rows = (size + strips - 1) // strips
    return strips, strips * (wp * (rows + 2 * r) + w8 * rows)


def fpga_section(fpga, base):
    sizes = sorted({k[2] for k in fpga})
    lines = ["## FPGA: memoria interna y enlace JTAG", "",
             "- **Memoria de datos del VPROC:** 8 bancos de un byte, pico de 8 bytes por ciclo (400 MB/s a 50 MHz). "
             "Cada `VLD` y `VST` mueve 8 bytes. Los bytes por píxel salen de la secuencia de instrucciones del modelo "
             "(validado contra los ciclos medidos) y el tiempo, del contador de hardware.",
             f"- **Enlace JTAG:** bytes que baja y sube el backend por imagen / (total − kernel). Incluye la conversión a texto y el "
             f"Tcl del lado de la PC. El máximo en bruto del cable es de unos {JTAG_TCK_MHZ / 8:.0f} MB/s ({JTAG_TCK_MHZ} MHz de TCK).", ""]
    series = {}
    per_px = {flt: stream_counts(flt) for flt in FILTERS}
    for flt in FILTERS:
        loads, stores = per_px[flt]
        lines += [f"### {flt}", "", f"Por píxel de salida: {loads:.0f} B leídos y {stores:.0f} B escritos en la memoria interna, contra "
                  f"1 B de entrada y 1 B de salida mínimos: la entrada se lee {loads:.0f} veces (sin reutilización de la ventana).", ""]
        rows = {k: [] for k in ("Kernel, T / tiempo (MB/s)", "Memoria interna (MB/s)", "Uso del pico de la memoria interna",
                                "Franjas", "Bytes por JTAG", "Enlace JTAG (KB/s)", "Enlace / máximo en bruto")}
        points_k, points_t = [], []
        for s in sizes:
            row = fpga.get(("fpga", flt, s))
            if not (row and row["kernel"]):
                for v in rows.values():
                    v.append("-")
                continue
            cycles = row["kernel"] * 1e-3 * CLOCK_HZ
            internal = (loads + stores) * s * s
            strips, moved = jtag_bytes(flt, s)
            link = moved / ((row["median"] - row["kernel"]) * 1e-3)
            rows["Kernel, T / tiempo (MB/s)"].append(cell(2 * s * s / (row["kernel"] * 1e-3) / 1e6, 1))
            rows["Memoria interna (MB/s)"].append(cell(internal / (row["kernel"] * 1e-3) / 1e6, 1))
            rows["Uso del pico de la memoria interna"].append(f"{internal / (cycles * DMEM_PEAK_BYTES_PER_CYCLE) * 100:.1f} %")
            rows["Franjas"].append(str(strips))
            rows["Bytes por JTAG"].append(f"{moved / 1e6:.2f} MB")
            rows["Enlace JTAG (KB/s)"].append(f"{link / 1e3:.0f}")
            rows["Enlace / máximo en bruto"].append(f"{link / (JTAG_TCK_MHZ / 8 * 1e6) * 100:.1f} %")
            points_k.append((s, 2 * s * s / (row["kernel"] * 1e-3) / 1e9))
            points_t.append((s, 2 * s * s / (row["median"] * 1e-3) / 1e9))
        series[(flt, "kernel")], series[(flt, "total")] = points_k, points_t
        lines += table([""] + [f"{s}²" for s in sizes], [[k] + v for k, v in rows.items()]) + [""]

    lines += ["### Propuesta 2 (estimada)", "",
              "Con los mismos bytes y los ciclos del modelo de pipeline, el uso del pico de la memoria interna sube:", ""]
    est = []
    for flt in FILTERS:
        loads, stores = per_px[flt]
        p1 = pipeline_model.analyse(flt, 128, 2)
        est.append([flt, f"{(loads + stores) * 128 ** 2 / (p1['p1'] * DMEM_PEAK_BYTES_PER_CYCLE) * 100:.1f} %",
                    f"{(loads + stores) * 128 ** 2 / (p1['p2'] * DMEM_PEAK_BYTES_PER_CYCLE) * 100:.1f} %"])
    return lines + table(["Filtro", "Propuesta 1 (modelo)", "Propuesta 2 (estimada)"], est) + [""], series


def plot(cpu_curves, gpu_series, fpga_series, out):
    fig, axes = plt.subplots(1, 2, figsize=(11, 5.4))
    for ax, flt in zip(axes, FILTERS):
        for label, points in cpu_curves.get(flt, {}).items():
            ax.plot(*zip(*points), marker="o", label=label)
        for (machine, backend, f, kind), points in sorted(gpu_series.items()):
            if f == flt and backend == "cuda_tiled":
                ax.plot(*zip(*points), marker="^", linestyle="-" if kind == "kernel" else ":",
                        label=f"cuda_tiled {kind} ({machine.split('-')[0]})")
        for kind, style in (("kernel", "-"), ("total", "--")):
            pts = fpga_series.get((flt, kind))
            if pts:
                ax.plot(*zip(*pts), marker="s", color="black", linestyle=style,
                        label="fpga kernel" if kind == "kernel" else "fpga con JTAG")
        ax.set(xscale="log", yscale="log", xlabel="lado de la imagen (px)", ylabel="2 x N² / tiempo (GB/s)",
               title=f"Filtro {flt}")
        ax.grid(True, which="both", alpha=0.3)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=4, fontsize=7)
    fig.suptitle("Ancho de banda efectivo (tráfico mínimo / tiempo)")
    fig.tight_layout(rect=[0, 0.14, 1, 0.95])
    fig.savefig(out, dpi=150)
    plt.close(fig)


def main():
    machines = {machine_name(p): load(p) for p in sorted((ROOT / "results").glob("*.csv"))}
    fpga, base, host = split_fpga_dir(ROOT / "results" / "fpga")
    lines = ["# Ancho de banda de memoria", "",
             "Todo sale de lo ya medido. No hay contadores de DRAM en la CPU ni en la FPGA, así que fuera de la "
             "GPU (Nsight) los valores son un mínimo derivado, y se indica en cada tabla.", ""]
    lines += cpu_section(machines, base, host)
    gpu_lines, gpu_series = gpu_section(machines)
    lines += gpu_lines
    fpga_lines, fpga_series = fpga_section(fpga, base) if fpga else ([], {})
    lines += fpga_lines
    cpu_curves = {}
    for flt in FILTERS:
        for name, data in {**machines, **({f'{host} (CPU de la FPGA)': base} if base else {})}.items():
            pts = []
            for s in sorted({k[2] for k in data}):
                pick = best(data, SIMD_BACKENDS, flt, s)
                if pick:
                    pts.append((s, gbs(2 * s * s, pick[1])))
            if pts:
                cpu_curves.setdefault(flt, {})[f"mejor SIMD ({name.split('-')[0]})"] = pts
    outdir = ROOT / "results" / "figuras"
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / "ancho_de_banda.md").write_text("\n".join(lines), encoding="utf-8")
    plot(cpu_curves, gpu_series, fpga_series, outdir / "ancho_de_banda.png")
    print(f"Escrito en {outdir}/ancho_de_banda.md y .png")


if __name__ == "__main__":
    main()
