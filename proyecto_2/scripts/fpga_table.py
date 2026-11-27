#!/usr/bin/env python3
"""Prints the results of an fpga backend CSV (from dlp_bench --csv) as an aligned table.

Usage:
    python3 scripts/fpga_table.py results/fpga/de1soc_20261123_1500.csv

Kernel time comes from the hardware cycle counter (cycles / 50 MHz). Total time is what the
runner measured and includes the JTAG transfers, so "Kernel/total" is the share of the run
that is real compute on the FPGA.
"""
import csv
import sys

CLOCK_KHZ = 50_000.0

HEADERS = ["Filtro", "Tamano", "Reps", "Ciclos", "Ciclos/px", "Kernel (ms)",
           "Total (ms)", "Kernel/total", "Error max", "Estado"]
RIGHT_ALIGNED = set(range(2, 9))


def to_float(text):
    try:
        return float(text)
    except (TypeError, ValueError):
        return None


def build_row(row):
    width, height = int(row["width"]), int(row["height"])
    kernel_ms = to_float(row["kernel_median_ms"])
    total_ms = to_float(row["median_ms"])
    cycles = round(kernel_ms * CLOCK_KHZ) if kernel_ms is not None else None
    share = kernel_ms / total_ms * 100 if kernel_ms is not None and total_ms else None
    error = to_float(row["max_abs_error"])
    status = {"1": "ok", "0": "FALLA"}.get(row["ok"], "-")
    return (row["filter"], width * height, [
        row["filter"],
        f"{width}x{height}",
        row["reps"],
        f"{cycles:,}" if cycles is not None else "-",
        f"{cycles / (width * height):.2f}" if cycles is not None else "-",
        f"{kernel_ms:.3f}" if kernel_ms is not None else "-",
        f"{total_ms:.1f}" if total_ms is not None else "-",
        f"{share:.1f} %" if share is not None else "-",
        f"{error:g}" if error is not None else "-",
        status,
    ])


def format_table(rows):
    widths = [max(len(HEADERS[i]), *(len(r[i]) for r in rows)) for i in range(len(HEADERS))]

    def line(cells):
        parts = [c.rjust(widths[i]) if i in RIGHT_ALIGNED else c.ljust(widths[i])
                 for i, c in enumerate(cells)]
        return "  ".join(parts).rstrip()

    separator = "  ".join("-" * w for w in widths)
    out = [line(HEADERS), separator]
    previous_filter = None
    for r in rows:
        if previous_filter is not None and r[0] != previous_filter:
            out.append(separator)
        out.append(line(r))
        previous_filter = r[0]
    return "\n".join(out)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], newline="") as f:
        entries = [build_row(r) for r in csv.DictReader(f) if r["backend"] == "fpga"]
    if not entries:
        sys.exit("El CSV no tiene filas del backend fpga")
    entries.sort(key=lambda e: (e[0], e[1]))
    print(f"Resultados FPGA ({sys.argv[1]})\n")
    print(format_table([e[2] for e in entries]))
    print("\nKernel = ciclos del contador de hardware / 50 MHz. Total = tiempo del runner, "
          "incluye las transferencias por JTAG.")


if __name__ == "__main__":
    main()
