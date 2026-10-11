#!/usr/bin/env python3
"""ht_mode.py - dice si Hyper-Threading (SMT) esta ACTIVO en esta maquina y a que
carpeta de results/ le toca guardar: results/ht_on o results/ht_off.

Los demas scripts lo usan para no mezclar las mediciones de HT encendido con las de HT
apagado (el enunciado pide compararlas). Orden de deteccion:
  1. variable de entorno HT=on|off  (para forzarlo a mano; ej: HT=off make bench-oficial)
  2. /sys/devices/system/cpu/smt/active   ("1" activo, "0" apagado)
  3. lscpu: si algun nucleo fisico tiene 2+ CPUs logicas, HT esta activo
  4. si nada funciona: se asume "on" y se avisa

Uso desde la terminal (proyecto_1/case_4):
    python3 scripts/ht_mode.py            # imprime: on   (o off)
    python3 scripts/ht_mode.py --root     # imprime: results/ht_on
"""
import os
import subprocess
import sys
from pathlib import Path

CASE4_DIR = Path(__file__).resolve().parent.parent  # proyecto_1/case_4
SUBDIRS = ("benchmark", "amdahl", "perfilado", "imagenes_raytracing")


def detect_mode():
    """Devuelve ('on' | 'off', de donde salio la respuesta)."""
    env = os.environ.get("HT", "").strip().lower()
    if env in ("on", "off"):
        return env, "variable de entorno HT"
    try:
        val = Path("/sys/devices/system/cpu/smt/active").read_text().strip()
        if val in ("0", "1"):
            return ("on" if val == "1" else "off"), "/sys/devices/system/cpu/smt/active"
    except OSError:
        pass
    try:
        out = subprocess.run(["lscpu", "-p=CPU,CORE"], capture_output=True, text=True, timeout=10).stdout
        per_core = {}
        for line in out.splitlines():
            if line and not line.startswith("#"):
                cpu, core = line.split(",")[:2]
                per_core[core] = per_core.get(core, 0) + 1
        if per_core:
            return ("on" if max(per_core.values()) >= 2 else "off"), "lscpu"
    except (OSError, subprocess.TimeoutExpired, ValueError):
        pass
    return "on", "sin deteccion (se asume on)"


def results_root(mode=None):
    """Carpeta absoluta results/ht_on o results/ht_off (no la crea)."""
    return CASE4_DIR / "results" / f"ht_{mode or detect_mode()[0]}"


def subdir(name, mode=None):
    """results/ht_<modo>/<name>, creada si no existe (name: benchmark, amdahl, perfilado, imagenes_raytracing)."""
    path = results_root(mode) / name
    path.mkdir(parents=True, exist_ok=True)
    return path


if __name__ == "__main__":
    mode, source = detect_mode()
    if "--root" in sys.argv:
        print(results_root(mode).relative_to(CASE4_DIR))
    else:
        print(mode)
    print(f"(HT {mode}: detectado por {source})", file=sys.stderr)
