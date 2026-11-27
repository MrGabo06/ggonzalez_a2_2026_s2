#!/usr/bin/env python3
"""Parallel efficiency of CUDA and of the FPGA.

Usage (from proyecto_2):
    python3 scripts/parallel_efficiency.py

Reads the benchmark CSVs (results/*.csv, results/fpga/*.csv) and the Nsight Compute reports
(results/perfilado/<machine>_ncu/) and writes results/figuras/eficiencia_paralela.md and .png.

Two metrics, defined in the output file:
  classic efficiency  E = S / p   S = median of seq / kernel time on the same machine and size,
                                  p = parallel lanes of the device (CUDA: SMs x FP32 lanes per SM;
                                  FPGA: the 8 lanes of the VPROC, S against the CPU that drives it)
  peak utilization    E_peak = useful MACs / (cycles x lanes), with cycles taken from Nsight Compute
                      (GPU) or from the hardware cycle counter (FPGA), so it does not depend on the
                      clock. Useful MACs = pixels x non-zero taps of the filter.
"""
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "fpga" / "scripts"))
from compare_platforms import ROOT, FILTERS, load, machine_name, split_fpga_dir  # noqa: E402
import pipeline_model  # noqa: E402

CUDA_BACKENDS = ["cuda", "cuda_tiled"]
FPGA_LANES = 8
FPGA_CLOCK_HZ = 50e6
# Non-zero taps: Gaussian 5x5 has 25; Sobel x 3x3 has 6 (the middle column is zero)
USEFUL_TAPS = {"gauss": 25, "sobel": 6}
# FP32 fused multiply-add lanes per SM and cycle, per compute capability
# (CUDA C Programming Guide, arithmetic instruction throughput table)
FP32_LANES_PER_SM = {"6.1": 128, "7.0": 64, "7.5": 64, "8.0": 64, "8.6": 128, "8.9": 128}


def parse_ncu(path):
    text = path.read_text(errors="replace")

    def grab(pattern):
        m = re.search(pattern, text)
        return float(m.group(1)) if m else None

    cc = re.search(r"CC (\d\.\d)", text)
    return {"cc": cc.group(1) if cc else None, "sms": grab(r"# SMs\s+SM\s+([\d.]+)"),
            "cycles": grab(r"Elapsed Cycles\s+cycle\s+([\d.]+)"),
            "sm_throughput": grab(r"Compute \(SM\) Throughput\s+%\s+([\d.]+)"),
            "ipc": grab(r"Executed Ipc Active\s+inst/cycle\s+([\d.]+)")}


def load_ncu(machine):
    """{(backend, filter, size): report} from results/perfilado/<machine>_ncu/."""
    reports = {}
    for path in (ROOT / "results" / "perfilado" / f"{machine}_ncu").glob("*.txt"):
        m = re.match(r"(cuda(?:_tiled)?)_(gauss|sobel)_(\d+)", path.stem)
        if m:
            reports[(m.group(1), m.group(2), int(m.group(3)))] = parse_ncu(path)
    return reports


def gpu_lanes(machine):
    """SMs x FP32 lanes per SM, taken from any Nsight report of the machine."""
    for report in load_ncu(machine).values():
        if report["sms"] and report["cc"] in FP32_LANES_PER_SM:
            return int(report["sms"]), report["cc"], int(report["sms"]) * FP32_LANES_PER_SM[report["cc"]]
    return None


def percent(value):
    return f"{value * 100:.2f} %" if value < 0.1 else f"{value * 100:.1f} %"


def cuda_classic(machines):
    lines = ["## CUDA: eficiencia paralela clásica (E = S / p)", "",
             "S es la mediana de `seq` entre el tiempo del kernel en la GPU (sin copias), en la misma "
             "máquina. Cada celda es `S / E`.", ""]
    series = {}
    for machine, data in machines.items():
        info = gpu_lanes(machine)
        if not info or not any(v["kernel"] for k, v in data.items() if k[0] in CUDA_BACKENDS):
            continue
        sms, cc, lanes = info
        sizes = sorted({k[2] for k in data if k[0] in CUDA_BACKENDS})
        lines += [f"### {machine}", "", f"GPU con {sms} SMs, capacidad {cc}: p = {sms} x {FP32_LANES_PER_SM[cc]} = {lanes} carriles FP32.", "",
                  "| Backend | Filtro | " + " | ".join(f"{s}²" for s in sizes) + " |", "|---|---|" + "---|" * len(sizes)]
        for backend in CUDA_BACKENDS:
            for flt in FILTERS:
                cells, points = [], []
                for size in sizes:
                    row, seq = data.get((backend, flt, size)), data.get(("seq", flt, size))
                    if row and row["kernel"] and seq:
                        s = seq["median"] / row["kernel"]
                        cells.append(f"{s:.0f}x / {percent(s / lanes)}")
                        points.append((size, s / lanes * 100))
                    else:
                        cells.append("-")
                if points:
                    series[(machine, backend, flt)] = points
                    lines.append(f"| {backend} | {flt} | " + " | ".join(cells) + " |")
        lines.append("")
    return lines, series


def cuda_peak(machines):
    lines = ["## CUDA: utilización del pico a 4096² (ciclos de Nsight Compute)", "",
             "E_peak = (píxeles x taps no nulos) / (ciclos transcurridos x SMs x carriles FP32 por SM). "
             "Un FMA cuenta como una MAC. Los ciclos no dependen del reloj, que Nsight fija. "
             "`SM Throughput` e `IPC` son los de Nsight, para comparar.", "",
             "| Máquina | Backend | Filtro | MAC útiles | Ciclos | E_peak | GMAC/s (kernel real) | SM Throughput | IPC |",
             "|---|---|---|---|---|---|---|---|---|"]
    for machine, data in machines.items():
        info = gpu_lanes(machine)
        for (backend, flt, size), rep in sorted(load_ncu(machine).items()):
            if not info or not rep["cycles"]:
                continue
            macs = size * size * USEFUL_TAPS[flt]
            peak = macs / (rep["cycles"] * info[2])
            kernel = (data.get((backend, flt, size)) or {}).get("kernel")
            gmacs = f"{macs / (kernel * 1e-3) / 1e9:.0f}" if kernel else "-"
            lines.append(f"| {machine} | {backend} | {flt} | {macs / 1e6:.0f} M | {rep['cycles'] / 1e6:.2f} M | "
                         f"{percent(peak)} | {gmacs} | {rep['sm_throughput']:.0f} % | {rep['ipc']:.2f} |")
    return lines + [""]


def fpga_section(fpga, base, host):
    sizes = sorted({k[2] for k in fpga})
    lines = ["## FPGA (VPROC de 8 carriles a 50 MHz)", "",
             f"- Clásica: S es la mediana de `seq` en {host} (la CPU que maneja la tarjeta) entre el tiempo de kernel "
             f"(ciclos / 50 MHz); p = {FPGA_LANES}. Compara un reloj de 50 MHz contra el de la CPU, así que "
             "E queda muy por debajo de 1.",
             f"- E_peak = (píxeles x taps no nulos) / (ciclos x {FPGA_LANES}). Con los ciclos del contador de hardware. "
             "Es la fracción de ciclos en que los 8 carriles hacen una MAC útil.",
             "- Estimada: ciclos de la propuesta 2 (pipeline) según `fpga/scripts/pipeline_model.py`, no medidos.", ""]
    series = {}
    for flt in FILTERS:
        lines += [f"### {flt}", "", "| | " + " | ".join(f"{s}²" for s in sizes) + " |", "|---|" + "---|" * len(sizes)]
        rows = {"S (kernel contra seq)": [], "E clásica": [], "GMAC/s": [], "E_peak, propuesta 1 (medida)": [],
                "E_peak, propuesta 2 (estimada)": []}
        points = []
        for size in sizes:
            row, seq = fpga.get(("fpga", flt, size)), base.get(("seq", flt, size))
            if not (row and row["kernel"]):
                for cells in rows.values():
                    cells.append("-")
                continue
            pixels, macs = size * size, size * size * USEFUL_TAPS[flt]
            cycles = row["kernel"] * 1e-3 * FPGA_CLOCK_HZ
            s = seq["median"] / row["kernel"] if seq else None
            rows["S (kernel contra seq)"].append(f"{s:.3f}x" if s else "-")
            rows["E clásica"].append(percent(s / FPGA_LANES) if s else "-")
            rows["GMAC/s"].append(f"{macs / (row['kernel'] * 1e-3) / 1e9:.3f}")
            rows["E_peak, propuesta 1 (medida)"].append(percent(macs / (cycles * FPGA_LANES)))
            p2 = pipeline_model.analyse(flt, size, 2)["p2"]
            rows["E_peak, propuesta 2 (estimada)"].append(percent(macs / (p2 * FPGA_LANES)))
            if s:
                points.append((size, s / FPGA_LANES * 100))
        for label, cells in rows.items():
            lines.append(f"| {label} | " + " | ".join(cells) + " |")
        lines.append("")
        series[flt] = points
    return lines, series


def definitions():
    return ["# Eficiencia paralela de CUDA y de la FPGA", "",
            "## Definiciones", "",
            "- **Speedup** S = mediana de `seq` / tiempo del dispositivo, mismo filtro, tamaño y máquina. En la GPU y "
            "la FPGA se usa solo el kernel (cudaEvent y contador de ciclos), sin copias ni JTAG.",
            "- **Eficiencia paralela clásica** E = S / p, con p el número de carriles paralelos del dispositivo. "
            "En SIMD, `analyze.py` la calcula contra su secuencial dividiendo entre 8 o 16 píxeles por instrucción.",
            "- **Utilización del pico** E_peak = MAC útiles / (ciclos x carriles). No depende del reloj y mide qué parte "
            "de la capacidad de cómputo hace trabajo útil. MAC útiles = píxeles x taps no nulos (Gauss 25, Sobel 6).",
            "- **Cómo leerlas.** E clásica compara contra un núcleo de CPU que corre a un reloj distinto, así que no "
            "es una fracción de la capacidad del dispositivo. E_peak sí lo es. Una E clásica baja con un speedup alto "
            "es normal en una GPU, donde p es de miles de carriles.", ""]


def plot(cuda_series, fpga_series, out):
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    for ax, flt in zip(axes, FILTERS):
        for (machine, backend, f), points in sorted(cuda_series.items()):
            if f == flt:
                ax.plot(*zip(*points), marker="o", label=f"{backend} ({machine.split('-')[0]})")
        if fpga_series.get(flt):
            ax.plot(*zip(*fpga_series[flt]), marker="s", color="black", label="fpga (kernel)")
        ax.set(xscale="log", yscale="log", xlabel="lado de la imagen (px)", ylabel="E = S / p (%)",
               title=f"Filtro {flt}")
        ax.grid(True, which="both", alpha=0.3)
    axes[1].legend(fontsize=8)
    fig.suptitle("Eficiencia paralela clásica de CUDA y de la FPGA")
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def main():
    machines = {machine_name(p): load(p) for p in sorted((ROOT / "results").glob("*.csv"))}
    fpga, base, host = split_fpga_dir(ROOT / "results" / "fpga")
    lines = definitions()
    cuda_lines, cuda_series = cuda_classic(machines)
    fpga_lines, fpga_series = fpga_section(fpga, base, host) if fpga and base else ([], {})
    lines += cuda_lines + cuda_peak(machines) + fpga_lines
    outdir = ROOT / "results" / "figuras"
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / "eficiencia_paralela.md").write_text("\n".join(lines), encoding="utf-8")
    plot(cuda_series, fpga_series, outdir / "eficiencia_paralela.png")
    print(f"Escrito en {outdir}/eficiencia_paralela.md y .png")


if __name__ == "__main__":
    main()
