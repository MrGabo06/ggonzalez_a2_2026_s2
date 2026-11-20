#!/usr/bin/env python3
"""Simula el VPROC con Icarus Verilog y valida la salida contra el baseline secuencial.

Uso (desde proyecto_2/):
    python3 fpga/scripts/run_sim.py --filter gauss --size 64
    python3 fpga/scripts/run_sim.py --filter sobel --size 32 --seed 7
    python3 fpga/scripts/run_sim.py --filter smoke          # ejecución dummy mínima
    python3 fpga/scripts/run_sim.py --filter isa            # prueba cada instrucción del ISA
    (o: make fpga_sim FILTER=gauss SIZE=64)

Pasos:
  1. dlp_bench --backend seq exporta la imagen de entrada y la salida del baseline en .hex
  2. vasm.py ensambla el programa con el tamaño y la distribución de memoria elegidos
  3. iverilog compila el RTL y el testbench; vvp ejecuta y compara píxel a píxel
"""
import argparse
import os
import random
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))  # proyecto_2/
FPGA = os.path.join(ROOT, "fpga")
BUILD = os.path.join(ROOT, "build", "fpga")
LANES = 8
DMEM_BYTES = 65536   # igual que tb_vproc.v
IMEM_WORDS = 512     # igual que tb_vproc.v

RADIUS = {"gauss": 2, "sobel": 1, "smoke": 0, "isa": 0}
PROGRAM = {"gauss": "gauss5.s", "sobel": "sobel3.s", "smoke": "smoke.s", "isa": "isa_test.s"}
ISA_EXPECTED = [2, 35, 80, 33, 0, 175, 255, 19]   # un valor por grupo de 8 bytes (ver isa_test.s)


def run(cmd, **kw):
    print("$ " + " ".join(cmd))
    return subprocess.run(cmd, check=True, **kw)


def baseline_hex(filt, size, seed):
    """Imagen de entrada y salida esperada usando el framework (mismo validador que CPU/GPU)."""
    bench = os.path.join(ROOT, "build", "dlp_bench")
    if not os.path.exists(bench):
        run(["make", "-C", ROOT, "build"])
    hex_dir = os.path.join(BUILD, "hex")
    run([bench, "--backend", "seq", "--size", str(size), "--filter", filt, "--seed", str(seed),
         "--reps", "1", "--warmup", "0", "--hex-dir", hex_dir], stdout=subprocess.DEVNULL)
    base = os.path.join(hex_dir, f"{filt}_{size}x{size}_seed{seed}")
    return base + "_in.hex", base + "_seq_out.hex"


def smoke_hex(seed):
    """16 píxeles aleatorios (incluye 255 para probar la saturación) y su resultado + 1."""
    rnd = random.Random(seed)
    pix = [rnd.randrange(256) for _ in range(15)] + [255]
    os.makedirs(BUILD, exist_ok=True)
    fin, fexp = os.path.join(BUILD, "smoke_in.hex"), os.path.join(BUILD, "smoke_exp.hex")
    with open(fin, "w") as f:
        f.writelines(f"{p:02X}\n" for p in pix)
    with open(fexp, "w") as f:
        f.writelines(f"{min(p + 1, 255):02X}\n" for p in pix)
    return fin, fexp


def isa_hex():
    """Entrada en cero y los 64 bytes que debe escribir isa_test.s."""
    os.makedirs(BUILD, exist_ok=True)
    fin, fexp = os.path.join(BUILD, "isa_in.hex"), os.path.join(BUILD, "isa_exp.hex")
    with open(fin, "w") as f:
        f.write("00\n" * 64)
    with open(fexp, "w") as f:
        f.writelines(f"{v:02X}\n" for v in ISA_EXPECTED for _ in range(LANES))
    return fin, fexp


def main():
    for tool in ("iverilog", "vvp"):
        if shutil.which(tool) is None:
            sys.exit(f"No se encontró '{tool}'. Instale Icarus Verilog: sudo apt install iverilog")

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--filter", choices=sorted(RADIUS), default="gauss")
    ap.add_argument("--size", type=int, default=32, help="lado de la imagen (múltiplo de 8)")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--vcd", action="store_true", help="guardar formas de onda en build/fpga/")
    ap.add_argument("--wave", action="store_true",
                    help="igual que --vcd y además abre gtkwave con la vista fpga/tb/vproc.gtkw")
    args = ap.parse_args()

    if args.filter == "smoke":
        w, h = 16, 1
        fin, fexp = smoke_hex(args.seed)
    elif args.filter == "isa":
        w, h = 64, 1
        fin, fexp = isa_hex()
    else:
        if args.size % LANES:
            sys.exit(f"--size debe ser múltiplo de {LANES}")
        w = h = args.size
        fin, fexp = baseline_hex(args.filter, args.size, args.seed)

    # Distribución de la memoria de datos: entrada con borde y luego la salida
    r = RADIUS[args.filter]
    wp, hp = w + 2 * r, h + 2 * r
    in_base, out_base = 0, wp * hp
    if out_base + w * h > DMEM_BYTES:
        sys.exit(f"la imagen no cabe en {DMEM_BYTES} bytes de memoria de datos; use un tamaño menor")

    os.makedirs(BUILD, exist_ok=True)
    tag = f"{args.filter}_{w}x{h}"
    prog_hex = os.path.join(BUILD, f"{tag}.prog.hex")
    run([sys.executable, os.path.join(FPGA, "asm", "vasm.py"), os.path.join(FPGA, "programs", PROGRAM[args.filter]),
         "-o", prog_hex, "-l", os.path.join(BUILD, f"{tag}.lst"),
         "-D", f"W={w}", "-D", f"H={h}", "-D", f"WP={wp}", "-D", f"IN={in_base}", "-D", f"OUT={out_base}"])

    # Rellena con NOP hasta el tamaño de la memoria de instrucciones (como la cargaría el host)
    with open(prog_hex) as f:
        n = sum(1 for _ in f)
    if n > IMEM_WORDS:
        sys.exit(f"el programa ocupa {n} palabras y la memoria de instrucciones tiene {IMEM_WORDS}")
    with open(prog_hex, "a") as f:
        f.write("00000000\n" * (IMEM_WORDS - n))

    sim = os.path.join(BUILD, "tb_vproc.vvp")
    rtl = [os.path.join(FPGA, "rtl", f) for f in sorted(os.listdir(os.path.join(FPGA, "rtl"))) if f.endswith(".v")]
    run(["iverilog", "-g2005", "-Wall", "-Wno-timescale", "-I", os.path.join(FPGA, "rtl"), "-o", sim,
         os.path.join(FPGA, "tb", "tb_vproc.v")] + rtl)

    plus = [f"+prog={prog_hex}", f"+in={fin}", f"+exp={fexp}", f"+out={os.path.join(BUILD, tag + '_fpga_out.hex')}",
            f"+W={w}", f"+H={h}", f"+R={r}", f"+IN={in_base}", f"+OUT={out_base}"]
    if args.wave:
        args.vcd = True
        if shutil.which("gtkwave") is None:
            sys.exit("No se encontró 'gtkwave'. Instálelo: sudo apt install gtkwave")
    if args.vcd:
        plus.append(f"+vcd={os.path.join(BUILD, tag + '.vcd')}")
    out = run(["vvp", "-n", sim] + plus, capture_output=True, text=True).stdout
    print(out, end="")
    passed = "PASS" in out.splitlines()
    if args.wave:
        vcd = os.path.join(BUILD, tag + ".vcd")
        run(["gtkwave", vcd, os.path.join(FPGA, "tb", "vproc.gtkw"),
             "-S", os.path.join(FPGA, "tb", "vproc_zoom.tcl")])
    sys.exit(0 if passed else 1)


if __name__ == "__main__":
    main()
