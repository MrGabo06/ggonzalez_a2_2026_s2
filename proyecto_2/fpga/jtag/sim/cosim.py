#!/usr/bin/env python3
"""Co-simulación del enlace JTAG completo sin tarjeta.

Arranca Icarus con tb_jtag_cosim.v (el top JTAG real con un modelo del Virtual JTAG) unido por
dos FIFOs al script vproc_jtag.tcl, y ejecuta un comando con las variables de entorno listas.
El comando puede ser el backend fpga de dlp_bench (con DLP_FPGA_SIM=1) o tclsh directo.

Uso (desde proyecto_2/):
    python3 fpga/jtag/sim/cosim.py -- build/dlp_bench --backend fpga --size 32 --validate
    (o: make fpga_jtag_sim)
"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
JTAG = os.path.join(ROOT, "fpga", "jtag")
BUILD = os.path.join(ROOT, "build", "fpga", "jtag_sim")


def main():
    if "--" not in sys.argv:
        sys.exit(__doc__)
    cmd = sys.argv[sys.argv.index("--") + 1:]
    for tool in ("iverilog", "vvp", "tclsh"):
        if shutil.which(tool) is None:
            sys.exit(f"No se encontró '{tool}' (sudo apt install iverilog tcl)")

    os.makedirs(BUILD, exist_ok=True)
    vvp = os.path.join(BUILD, "tb_jtag_cosim.vvp")
    rtl = os.path.join(ROOT, "fpga", "rtl")
    srcs = [os.path.join(JTAG, "sim", "tb_jtag_cosim.v"), os.path.join(JTAG, "sim", "sld_virtual_jtag.v"),
            os.path.join(JTAG, "de1soc_jtag_top.v"), os.path.join(JTAG, "vjtag_bridge.v"),
            os.path.join(ROOT, "fpga", "board", "de1soc_display.v")]
    srcs += [os.path.join(rtl, f) for f in sorted(os.listdir(rtl)) if f.endswith(".v")]
    subprocess.run(["iverilog", "-g2005", "-Wall", "-Wno-timescale", "-I", rtl, "-o", vvp] + srcs, check=True)

    tmp = tempfile.mkdtemp(prefix="vproc_jtag_")
    req, resp = os.path.join(tmp, "req"), os.path.join(tmp, "resp")
    os.mkfifo(req)
    os.mkfifo(resp)
    sim = subprocess.Popen(["vvp", "-n", vvp, f"+req={req}", f"+resp={resp}"])

    env = dict(os.environ, VPROC_JTAG_SIM_REQ=req, VPROC_JTAG_SIM_RESP=resp, DLP_FPGA_SIM="1")
    try:
        rc = subprocess.run(cmd, env=env).returncode
    finally:
        try:
            sim.wait(timeout=30)
        except subprocess.TimeoutExpired:
            sim.kill()
        shutil.rmtree(tmp, ignore_errors=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
