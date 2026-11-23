#!/usr/bin/env python3
"""Cycle model of the VPROC: proposal 1 (multicycle, measured) and proposal 2 (4-stage pipeline).

Usage (from proyecto_2):
    python3 fpga/scripts/pipeline_model.py
    python3 fpga/scripts/pipeline_model.py --sizes 32 64 512 --branch-penalty 1

The programs of fpga/programs/ are assembled with fpga/asm/vasm.py and executed by a functional
model that only tracks the scalar registers, which are the only ones that decide control flow. That
gives the exact dynamic instruction stream of the real program. Over that stream:
  - Proposal 1 takes 3 cycles per instruction and 4 for VLD, as in fpga/rtl/vproc_control.v. The
    model is checked against the cycle counts measured on the board (results/fpga/*.csv).
  - Proposal 2 is an in-order pipeline IF, ID, EX, MEM/WB with forwarding. Stalls come from the
    load-use dependency on VLD (data arrives one stage later than an ALU result) and from taken
    branches resolved in EX (the two instructions fetched behind them are squashed).
It is an estimate of a design that does not exist yet, not a measurement.
"""
import argparse
import csv
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASM = ROOT / "fpga" / "asm" / "vasm.py"
PROGRAMS = {"gauss": ("gauss5.s", 2), "sobel": ("sobel3.s", 1)}
CLOCK_HZ = 50e6
MASK = 0xFFFFFFFF

HALT, ADD, SUB, MUL, ADDI, SLLI, RDCYC, BNE, BLT, JMP = 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A
VLD, VST, VADD, VSUB, VMULS, VMACS, VADDI, VSRAI, VABS, VBCAST = 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19


def decode(word):
    imm = word & 0x3FFF
    return {"op": word >> 26, "rd": (word >> 22) & 0xF, "rs": (word >> 18) & 0xF,
            "rt": (word >> 14) & 0xF, "imm": imm - 0x4000 if imm & 0x2000 else imm}


def register_use(i):
    """(destination, sources) as ('r'|'v', number) tokens; r0 never creates a dependency."""
    op, rd, rs, rt = i["op"], i["rd"], i["rs"], i["rt"]
    if op in (ADD, SUB, MUL):
        return ("r", rd), [("r", rs), ("r", rt)]
    if op in (ADDI, SLLI):
        return ("r", rd), [("r", rs)]
    if op == RDCYC:
        return ("r", rd), []
    if op in (BNE, BLT):
        return None, [("r", rs), ("r", rt)]
    if op == VLD:
        return ("v", rd & 7), [("r", rs)]
    if op == VST:
        return None, [("v", rd & 7), ("r", rs)]
    if op in (VADD, VSUB):
        return ("v", rd & 7), [("v", rs & 7), ("v", rt & 7)]
    if op == VMULS:
        return ("v", rd & 7), [("v", rs & 7), ("r", rt)]
    if op == VMACS:
        return ("v", rd & 7), [("v", rd & 7), ("v", rs & 7), ("r", rt)]
    if op in (VADDI, VSRAI, VABS):
        return ("v", rd & 7), [("v", rs & 7)]
    if op == VBCAST:
        return ("v", rd & 7), [("r", rs)]
    return None, []


def assemble(filter_name, size):
    source, radius = PROGRAMS[filter_name]
    width = (size + 7) // 8 * 8
    wp = width + 2 * radius
    defines = {"W": width, "H": size, "WP": wp, "IN": 0, "OUT": wp * (size + 2 * radius)}
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "prog.hex"
        cmd = [sys.executable, str(ASM), str(ROOT / "fpga" / "programs" / source), "-o", str(out)]
        for name, value in defines.items():
            cmd += ["-D", f"{name}={value}"]
        subprocess.run(cmd, check=True, capture_output=True)
        return [decode(int(line, 16)) for line in out.read_text().split() if line.strip()]


def run_stream(program):
    """Functional execution of the control flow; returns the list of executed pcs."""
    regs, pc, stream = [0] * 16, 0, []
    while True:
        i = program[pc]
        stream.append(pc)
        op = i["op"]
        if op == HALT:
            return stream
        rd, rs, rt, imm = i["rd"], i["rs"], i["rt"], i["imm"]
        taken = False
        if op == ADD:
            regs[rd] = (regs[rs] + regs[rt]) & MASK
        elif op == SUB:
            regs[rd] = (regs[rs] - regs[rt]) & MASK
        elif op == MUL:
            regs[rd] = (regs[rs] * regs[rt]) & MASK
        elif op == ADDI:
            regs[rd] = (regs[rs] + imm) & MASK
        elif op == SLLI:
            regs[rd] = (regs[rs] << imm) & MASK
        elif op == RDCYC:
            regs[rd] = 0
        elif op == BNE:
            taken = regs[rs] != regs[rt]
        elif op == BLT:
            taken = (regs[rs] ^ 0x80000000) < (regs[rt] ^ 0x80000000)
        elif op == JMP:
            taken = True
        regs[0] = 0
        pc += imm if taken else 1


def analyse(filter_name, size, branch_penalty):
    program = assemble(filter_name, size)
    stream = run_stream(program)
    vld = sum(program[pc]["op"] == VLD for pc in stream)
    p1 = 3 * len(stream) + vld

    uses = [register_use(i) for i in program]
    ready, ex_prev, floor = {}, 1, 0
    load_stalls = bubbles = 0
    for n, pc in enumerate(stream):
        i = program[pc]
        dest, sources = uses[pc]
        earliest = max(ex_prev + 1, floor)
        needed = max((ready.get(s, 0) for s in sources if s != ("r", 0)), default=0)
        ex = max(earliest, needed)
        load_stalls += ex - earliest
        if dest and dest != ("r", 0):
            ready[dest] = ex + (2 if i["op"] == VLD else 1)
        ex_prev, floor = ex, 0
        is_taken = n + 1 < len(stream) and stream[n + 1] != pc + 1 and i["op"] in (BNE, BLT, JMP)
        if is_taken:
            floor = ex + 1 + branch_penalty
            bubbles += branch_penalty
    p2 = ex_prev + 1
    return {"instructions": len(stream), "vld": vld, "p1": p1, "p2": p2,
            "load_stalls": load_stalls, "branch_bubbles": bubbles, "pixels": size * size}


def measured_cycles():
    """{(filter, size): cycles} from the FPGA benchmark CSV, one strip sizes only."""
    best = {}
    for path in sorted((ROOT / "results" / "fpga").glob("*.csv")):
        with open(path, newline="") as f:
            for r in csv.DictReader(f):
                if r["backend"] == "fpga" and r["kernel_median_ms"]:
                    best[(r["filter"], int(r["width"]))] = round(float(r["kernel_median_ms"]) * CLOCK_HZ / 1000)
    return best


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--sizes", type=int, nargs="+", default=[32, 64, 128, 512])
    parser.add_argument("--branch-penalty", type=int, default=2,
                        help="bubbles after a taken branch (2 = resolved in EX, 1 = resolved in ID)")
    args = parser.parse_args()
    measured = measured_cycles()

    print(f"Modelo de ciclos del VPROC (penalizacion de salto tomado: {args.branch_penalty} burbujas)\n")
    header = ["Filtro", "Tam.", "Instr.", "VLD", "P1 modelo", "P1 medido", "P2 ciclos", "Paradas load-use",
              "Burbujas salto", "P2 sin paradas", "CPI P2", "Ciclos/px P1", "Ciclos/px P2", "Mejora P2"]
    rows = []
    for flt in PROGRAMS:
        for size in args.sizes:
            r = analyse(flt, size, args.branch_penalty)
            real = measured.get((flt, size)) if size % 8 == 0 and size <= 128 else None
            rows.append([flt, f"{size}²", r["instructions"], r["vld"], r["p1"],
                         f"{real} ({r['p1'] - real:+d})" if real else "-", r["p2"], r["load_stalls"],
                         r["branch_bubbles"], r["p2"] - r["load_stalls"], f"{r['p2'] / r['instructions']:.2f}",
                         f"{r['p1'] / r['pixels']:.2f}", f"{r['p2'] / r['pixels']:.2f}", f"{r['p1'] / r['p2']:.2f}x"])
    widths = [max(len(str(x)) for x in col) for col in zip(header, *rows)]
    for row in [header] + rows:
        print("  ".join(str(c).rjust(w) if n >= 1 else str(c).ljust(w) for n, (c, w) in enumerate(zip(row, widths))))
    print("\nP1 modelo = 3 ciclos por instruccion + 1 por cada VLD. El entre parentesis es la diferencia "
          "contra los ciclos medidos en la tarjeta (solo tamanos de una franja).")
    print("P2 sin paradas = cota si el compilador/ensamblador ocultara todas las paradas load-use.")


if __name__ == "__main__":
    main()
