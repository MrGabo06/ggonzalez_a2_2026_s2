#!/usr/bin/env python3
"""Profiles one VPROC run from a SignalTap CSV export (fpga/jtag/vproc_jtag.stp).

Usage:
    python3 scripts/signaltap_report.py results/signaltap/sobel_32.csv
    python3 scripts/signaltap_report.py results/signaltap/sobel_32.csv --cycles 9942 --md out.md

Expected signals in the CSV: busy, error, done, start, opcode[5..0], state[2..0], pc[8..0].
The run is the window where busy is high. Every instruction has exactly one EXEC cycle, so
EXEC samples give the instruction count, their opcode gives the instruction mix and their pc
gives the hot loop. MEM samples are the extra cycle of each VLD.
"""
import argparse
import csv
from collections import Counter
from pathlib import Path

FETCH, DECODE, EXEC, MEM = 1, 2, 3, 4
STATE_NAMES = {FETCH: "FETCH", DECODE: "DECODE", EXEC: "EXEC", MEM: "MEM"}

MNEMONICS = {
    0x00: "NOP", 0x01: "HALT", 0x02: "ADD", 0x03: "SUB", 0x04: "MUL", 0x05: "ADDI",
    0x06: "SLLI", 0x07: "RDCYC", 0x08: "BNE", 0x09: "BLT", 0x0A: "JMP", 0x10: "VLD",
    0x11: "VST", 0x12: "VADD", 0x13: "VSUB", 0x14: "VMULS", 0x15: "VMACS", 0x16: "VADDI",
    0x17: "VSRAI", 0x18: "VABS", 0x19: "VBCAST",
}
CLASSES = {
    "control": {0x00, 0x01},
    "escalar": {0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
    "salto": {0x08, 0x09, 0x0A},
    "memoria": {0x10, 0x11},
    "vectorial": {0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19},
}
VLD = 0x10


def load_samples(path):
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    header_at = next(i for i, r in enumerate(rows) if r and r[0].startswith("time unit"))
    names = [c.strip() for c in rows[header_at]]

    def column(suffix, exact=None):
        matches = [i for i, n in enumerate(names)
                   if (n == exact if exact else n.endswith(suffix))]
        if not matches:
            raise SystemExit(f"El CSV no tiene la columna '{exact or suffix}'")
        return matches[0]

    cols = {
        "busy": column("", exact="vproc_core:u_core|busy"),
        "error": column("", exact="vproc_core:u_core|error"),
        "done": column("", exact="vproc_core:u_core|done"),
        "start": column("", exact="vproc_core:u_core|start"),
        "opcode": column("|opcode[5..0]"),
        "state": column("|state[2..0]"),
        "pc": column("|pc[8..0]"),
    }
    samples = []
    for row in rows[header_at + 1:]:
        if len(row) < len(names) or "X" in "".join(row):
            continue
        cells = [c.strip() for c in row]
        samples.append({
            "busy": int(cells[cols["busy"]]), "error": int(cells[cols["error"]]),
            "done": int(cells[cols["done"]]), "start": int(cells[cols["start"]]),
            "opcode": int(cells[cols["opcode"]], 16), "state": int(cells[cols["state"]], 16),
            "pc": int(cells[cols["pc"]], 16), "index": int(cells[0]),
        })
    return samples


def find_run(samples):
    start_at = next((i for i, s in enumerate(samples) if s["start"]), None)
    if start_at is None:
        raise SystemExit("La captura no tiene un pulso de start")
    first_busy = next((i for i in range(start_at, len(samples)) if samples[i]["busy"]), None)
    if first_busy is None:
        raise SystemExit("busy nunca subio despues de start")
    end = first_busy
    while end < len(samples) and samples[end]["busy"]:
        end += 1
    completed = end < len(samples) and samples[end]["done"] == 1
    return start_at, first_busy, end, completed


def percent(part, whole):
    return f"{100.0 * part / whole:.1f} %"


def table(headers, rows, right_from=1):
    widths = [max(len(str(r[i])) for r in [headers] + rows) for i in range(len(headers))]
    def line(cells):
        return "  ".join(str(c).rjust(widths[i]) if i >= right_from else str(c).ljust(widths[i])
                         for i, c in enumerate(cells))
    return [line(headers), "  ".join("-" * w for w in widths)] + [line(r) for r in rows]


def report(samples, expected_cycles, source):
    start_at, first_busy, end, completed = find_run(samples)
    run = samples[first_busy:end]
    cycles = len(run)
    out = [f"Perfil de la ejecucion ({source})", ""]

    out.append("Ventana de la ejecucion")
    out += table(["Dato", "Valor"], [
        ["Muestra del pulso start", samples[start_at]["index"]],
        ["Muestras con busy = 1", cycles],
        ["Ciclos start -> done", samples[end]["index"] - samples[start_at]["index"] if completed else "n/d"],
        ["Termino con done", "si" if completed else "NO (captura cortada)"],
        ["Error en la ejecucion", "si" if any(s["error"] for s in run) else "no"],
    ])
    if expected_cycles is not None:
        diff = cycles - expected_cycles
        out += ["", f"Contador de hardware: {expected_cycles} ciclos. Diferencia con las muestras de busy: {diff:+d}"]
    out.append("")

    by_state = Counter(s["state"] for s in run)
    instructions = by_state[EXEC]
    out.append("Ocupacion por estado de la FSM")
    out += table(["Estado", "Ciclos", "% del total"],
                 [[STATE_NAMES[k], by_state[k], percent(by_state[k], cycles)]
                  for k in (FETCH, DECODE, EXEC, MEM)])
    other = cycles - sum(by_state[k] for k in STATE_NAMES)
    if other:
        out.append(f"(hay {other} ciclos en otros estados)")
    out.append("")

    exec_samples = [s for s in run if s["state"] == EXEC]
    mix = Counter(s["opcode"] for s in exec_samples)
    vld_count = mix[VLD]
    out.append("Rendimiento")
    out += table(["Dato", "Valor"], [
        ["Instrucciones ejecutadas", instructions],
        ["CPI medido (ciclos / instrucciones)", f"{cycles / instructions:.3f}"],
        ["VLD ejecutadas", vld_count],
        ["Ciclos MEM (deben ser iguales a VLD)", by_state[MEM]],
        ["Ciclos si todas fueran de 3 etapas", 3 * instructions],
        ["Ciclos extra por VLD", by_state[MEM]],
    ])
    out.append("")

    out.append("Mezcla de instrucciones (ciclos = 3 por instruccion + 1 por cada VLD)")
    mix_rows = []
    for opcode, count in sorted(mix.items(), key=lambda kv: -kv[1]):
        cyc = count * (4 if opcode == VLD else 3)
        mix_rows.append([MNEMONICS.get(opcode, f"0x{opcode:02X}"), count,
                         percent(count, instructions), cyc, percent(cyc, cycles)])
    out += table(["Instruccion", "Veces", "% instr.", "Ciclos", "% ciclos"], mix_rows)
    out.append("")

    out.append("Por clase")
    class_rows = []
    for name, members in CLASSES.items():
        count = sum(mix[o] for o in members)
        cyc = 3 * count + (vld_count if name == "memoria" else 0)
        class_rows.append([name, count, percent(count, instructions), cyc, percent(cyc, cycles)])
    out += table(["Clase", "Instr.", "% instr.", "Ciclos", "% ciclos"], class_rows)
    out.append("")

    visits = Counter(s["pc"] for s in exec_samples)
    opcode_at = {s["pc"]: s["opcode"] for s in exec_samples}
    cycles_at = {pc: v * (4 if opcode_at[pc] == VLD else 3) for pc, v in visits.items()}
    groups = {}
    for pc, v in visits.items():
        groups.setdefault(v, []).append(pc)
    out.append("Bucles: PCs agrupados por numero de ejecuciones")
    loop_rows = []
    for v in sorted(groups, key=lambda v: -sum(cycles_at[p] for p in groups[v]))[:4]:
        group_cycles = sum(cycles_at[p] for p in groups[v])
        loop_rows.append([v, len(groups[v]), f"{min(groups[v])}..{max(groups[v])}",
                          group_cycles, percent(group_cycles, cycles)])
    out += table(["Ejecuciones", "Instrucciones", "PCs", "Ciclos", "% ciclos"], loop_rows)
    hottest = max(groups, key=lambda v: sum(cycles_at[p] for p in groups[v]))
    inner_vld = sum(1 for p in groups[hottest] if opcode_at[p] == VLD)
    out.append(f"Bucle interno: {len(groups[hottest])} instrucciones ({inner_vld} VLD) x {hottest} "
               f"iteraciones = {3 * len(groups[hottest]) + inner_vld} ciclos por iteracion")
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("csv", help="CSV exportado desde SignalTap")
    parser.add_argument("--cycles", type=int, help="ciclos del contador de hardware, para cruzar")
    parser.add_argument("--md", help="guarda tambien el reporte como Markdown en este archivo")
    args = parser.parse_args()

    lines = report(load_samples(args.csv), args.cycles, args.csv)
    print("\n".join(lines))
    if args.md:
        Path(args.md).write_text("```\n" + "\n".join(lines) + "\n```\n")
        print(f"\nReporte guardado en {args.md}")


if __name__ == "__main__":
    main()
