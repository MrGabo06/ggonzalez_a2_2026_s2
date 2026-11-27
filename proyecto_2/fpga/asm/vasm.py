#!/usr/bin/env python3
"""Ensamblador del VPROC (ISA en fpga/ISA.md).

Uso:
    vasm.py programa.s -o programa.hex [-D NOMBRE=valor ...] [-l listado.lst]

Sintaxis:
    etiqueta:  OPCODE operandos       # comentario (también ';')
    .equ NOMBRE expresión             # constante
    Registros escalares r0..r15, vectoriales v0..v7.
    Memoria: VLD v1, 4(r8)  /  VST v0, 0(r6)   (desplazamiento puede ser expresión)
    Inmediatos y desplazamientos aceptan expresiones con constantes: WP*2+1
    Pseudoinstrucciones:
        LI  rd, valor   -> ADDI/SLLI/ADDI (siempre 3 palabras, valor de 0 a 2^25-1)
        MOV rd, rs      -> ADDI rd, rs, 0

Salida: una palabra de 32 bits por línea en hex (formato de $readmemh).
"""
import argparse
import re
import sys

# Mismos códigos que fpga/rtl/vproc_defs.vh
OPCODES = {
    "NOP": 0x00, "HALT": 0x01,
    "ADD": 0x02, "SUB": 0x03, "MUL": 0x04, "ADDI": 0x05, "SLLI": 0x06, "RDCYC": 0x07,
    "BNE": 0x08, "BLT": 0x09, "JMP": 0x0A,
    "VLD": 0x10, "VST": 0x11, "VADD": 0x12, "VSUB": 0x13, "VMULS": 0x14, "VMACS": 0x15,
    "VADDI": 0x16, "VSRAI": 0x17, "VABS": 0x18, "VBCAST": 0x19,
}

# Operandos de cada instrucción: R = escalar, V = vector, I = inmediato, M = memoria imm(rs),
# L = etiqueta (salto relativo). El orden de campos destino se resuelve en encode().
FORMATS = {
    "NOP": "", "HALT": "",
    "ADD": "RRR", "SUB": "RRR", "MUL": "RRR", "ADDI": "RRI", "SLLI": "RRI", "RDCYC": "R",
    "BNE": "RRL", "BLT": "RRL", "JMP": "L",
    "VLD": "VM", "VST": "VM",
    "VADD": "VVV", "VSUB": "VVV", "VMULS": "VVR", "VMACS": "VVR",
    "VADDI": "VVI", "VSRAI": "VVI", "VABS": "VV", "VBCAST": "VR",
}

IMM_MIN, IMM_MAX = -(1 << 13), (1 << 13) - 1


class AsmError(Exception):
    pass


def evaluate(expr, consts, line_no):
    """Evalúa una expresión entera usando solo constantes conocidas y + - * / ( ) << >>."""
    tokens = re.findall(r"[A-Za-z_]\w*|0x[0-9A-Fa-f]+|\d+|<<|>>|[-+*/()%]", expr)
    if "".join(tokens) != re.sub(r"\s+", "", expr):
        raise AsmError(f"línea {line_no}: expresión inválida '{expr}'")
    out = []
    for t in tokens:
        if re.match(r"[A-Za-z_]", t):
            if t not in consts:
                raise AsmError(f"línea {line_no}: constante desconocida '{t}'")
            out.append(str(consts[t]))
        elif t == "/":
            out.append("//")
        else:
            out.append(str(int(t, 0)) if t[0].isdigit() else t)
    return int(eval("".join(out), {"__builtins__": {}}, {}))


def reg(tok, kind, line_no):
    m = re.fullmatch(r"([rv])(\d+)", tok.strip().lower())
    if not m or m.group(1) != kind:
        raise AsmError(f"línea {line_no}: se esperaba registro {kind}N y llegó '{tok}'")
    n = int(m.group(2))
    if n > (15 if kind == "r" else 7):
        raise AsmError(f"línea {line_no}: registro fuera de rango '{tok}'")
    return n


def encode(op, rd=0, rs=0, rt=0, imm=0, line_no=0):
    if not IMM_MIN <= imm <= IMM_MAX:
        raise AsmError(f"línea {line_no}: inmediato {imm} no cabe en 14 bits con signo")
    return (OPCODES[op] << 26) | (rd << 22) | (rs << 18) | (rt << 14) | (imm & 0x3FFF)


def parse(lines, consts):
    """Primera pasada: expande pseudoinstrucciones, recoge etiquetas y constantes."""
    items, labels = [], {}
    for line_no, raw in enumerate(lines, 1):
        text = re.split(r"[#;]", raw, maxsplit=1)[0].strip()
        while True:
            m = re.match(r"([A-Za-z_]\w*)\s*:\s*(.*)", text)
            if not m:
                break
            labels[m.group(1)] = len(items)
            text = m.group(2).strip()
        if not text:
            continue
        parts = text.split(None, 1)
        mnem = parts[0].upper()
        args = [a.strip() for a in parts[1].split(",")] if len(parts) > 1 else []

        if mnem == ".EQU":
            name, expr = parts[1].split(None, 1)
            consts[name] = evaluate(expr, consts, line_no)
        elif mnem == "LI":
            # rd = valor en 3 palabras: hi (13 bits) << 12, más lo (12 bits)
            items.append(("LI", args, line_no))
            items.append(None)
            items.append(None)
        elif mnem == "MOV":
            items.append(("ADDI", [args[0], args[1], "0"], line_no))
        elif mnem in OPCODES:
            items.append((mnem, args, line_no))
        else:
            raise AsmError(f"línea {line_no}: instrucción desconocida '{parts[0]}'")
    return items, labels


def assemble(source, defines):
    consts = dict(defines)
    items, labels = parse(source.splitlines(), consts)
    words, listing = [], []
    for pc, item in enumerate(items):
        if item is None:
            continue  # palabras ya generadas por LI
        mnem, args, line_no = item
        if mnem == "LI":
            rd = reg(args[0], "r", line_no)
            value = evaluate(args[1], consts, line_no)
            if not 0 <= value < (1 << 25):
                raise AsmError(f"línea {line_no}: LI solo acepta 0..2^25-1")
            seq = [encode("ADDI", rd, 0, 0, value >> 12, line_no),
                   encode("SLLI", rd, rd, 0, 12, line_no),
                   encode("ADDI", rd, rd, 0, value & 0xFFF, line_no)]
            for k, w in enumerate(seq):
                words.append(w)
                listing.append(f"{pc + k:04d}  {w:08X}  {'LI ' + ', '.join(args) if k == 0 else ''}")
            continue

        fmt = FORMATS[mnem]
        if len(args) != len(fmt):
            raise AsmError(f"línea {line_no}: {mnem} espera {len(fmt)} operandos")
        rd = rs = rt = imm = 0
        if fmt == "RRR":
            rd, rs, rt = (reg(a, "r", line_no) for a in args)
        elif fmt == "RRI":
            rd, rs = reg(args[0], "r", line_no), reg(args[1], "r", line_no)
            imm = evaluate(args[2], consts, line_no)
        elif fmt == "R":
            rd = reg(args[0], "r", line_no)
        elif fmt in ("RRL", "L"):
            target = args[-1]
            if target not in labels:
                raise AsmError(f"línea {line_no}: etiqueta desconocida '{target}'")
            imm = labels[target] - pc
            if fmt == "RRL":
                rs, rt = reg(args[0], "r", line_no), reg(args[1], "r", line_no)
        elif fmt == "VM":
            rd = reg(args[0], "v", line_no)
            m = re.fullmatch(r"(.*)\(\s*(r\d+)\s*\)", args[1])
            if not m:
                raise AsmError(f"línea {line_no}: se esperaba desplazamiento(rN)")
            imm = evaluate(m.group(1) or "0", consts, line_no)
            rs = reg(m.group(2), "r", line_no)
        elif fmt == "VVV":
            rd, rs, rt = (reg(a, "v", line_no) for a in args)
        elif fmt == "VVR":
            rd, rs, rt = reg(args[0], "v", line_no), reg(args[1], "v", line_no), reg(args[2], "r", line_no)
        elif fmt == "VVI":
            rd, rs = reg(args[0], "v", line_no), reg(args[1], "v", line_no)
            imm = evaluate(args[2], consts, line_no)
        elif fmt == "VV":
            rd, rs = reg(args[0], "v", line_no), reg(args[1], "v", line_no)
        elif fmt == "VR":
            rd, rs = reg(args[0], "v", line_no), reg(args[1], "r", line_no)
        w = encode(mnem, rd, rs, rt, imm, line_no)
        words.append(w)
        listing.append(f"{pc:04d}  {w:08X}  {mnem} {', '.join(args)}")
    return words, listing


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("-l", "--listing")
    ap.add_argument("-D", action="append", default=[], metavar="NOMBRE=valor")
    args = ap.parse_args()

    defines = {}
    for d in args.D:
        name, value = d.split("=", 1)
        defines[name] = int(value, 0)

    try:
        with open(args.source, encoding="utf-8") as f:
            words, listing = assemble(f.read(), defines)
    except AsmError as e:
        sys.exit(f"{args.source}: {e}")

    with open(args.output, "w") as f:
        f.writelines(f"{w:08X}\n" for w in words)
    if args.listing:
        with open(args.listing, "w") as f:
            f.write("\n".join(listing) + "\n")
    print(f"{args.source}: {len(words)} instrucciones -> {args.output}")


if __name__ == "__main__":
    main()
