#!/usr/bin/env python3
"""bench.py - corre los motores del ray tracer (case_4) muchas veces y
guarda los tiempos en un CSV, para despues calcular speedup, eficiencia,
escalabilidad e intervalos de confianza.

Motores medidos:
  sequential, coarseGrained, fineGrained, cmp  -> barrido de --threads.
  smt       -> 2 hilos fijados a 2 CPUs logicas HERMANAS (mismo nucleo fisico).
  smt_ctrl  -> control: los mismos 2 hilos (mismo binario `smt`), pero en 2
               nucleos fisicos distintos. smt contra smt_ctrl aisla el efecto
               de compartir nucleo (SMT) sin tocar la BIOS.
smt/smt_ctrl solo corren en Linux fisico con el binario build/smt compilado;
los IDs de CPU se detectan con scripts/detect_cpus.sh (o se fuerzan con
--smt-cpus / --smt-ctrl-cpus). Si no hay pareja hermana (SMT apagado), smt se
omite y se avisa; smt_ctrl se mide igual.

Uso basico (desde proyecto_1/case_4):
    python3 scripts/bench.py --runs 5
Para la corrida oficial (200+ repeticiones):
    python3 scripts/bench.py --runs 200 --out results/bench_oficial.csv
"""
import argparse
import csv
import statistics
import subprocess
import sys
from pathlib import Path

CASE4_DIR = Path(__file__).resolve().parent.parent  # proyecto_1/case_4
BUILD_DIR = CASE4_DIR / "build"
DETECT_SCRIPT = CASE4_DIR / "scripts" / "detect_cpus.sh"

# Motores que toman <width> <height> <spheres> <depth> <threads> [salida.bmp]
THREADED_ENGINES = ["coarseGrained", "fineGrained", "cmp"]
# sequential no tiene parametro de threads: <width> <height> <spheres> <depth> [salida.bmp]
SEQUENTIAL = "sequential"
# smt toma <width> <height> <spheres> <depth> <ids_cpu_csv> [salida.bmp]; sus
# hilos = cantidad de IDs. En el CSV se etiqueta con threads = cantidad de IDs.
SMT = "smt"
SMT_CTRL = "smt_ctrl"


def run_once(binary: Path, args: list[str]) -> float:
    """Corre un binario, parsea la fila CSV que imprime y devuelve time_sec."""
    proc = subprocess.run([str(binary), *args], capture_output=True, text=True, timeout=300)
    if proc.returncode != 0:
        raise RuntimeError(f"{binary.name} {' '.join(args)} fallo (exit {proc.returncode}): {proc.stderr.strip()}")
    lines = [l for l in proc.stdout.strip().splitlines() if l.strip()]
    if len(lines) < 2:
        raise RuntimeError(f"{binary.name}: salida inesperada: {proc.stdout!r}")
    row = lines[-1].split(",")
    return float(row[-1])


def detect_cpus() -> dict:
    """Corre detect_cpus.sh y devuelve {'sibling': '3,7', 'separate': '0,1'}
    con lo que haya encontrado (vacio si no hay lscpu, ej. macOS)."""
    try:
        proc = subprocess.run(["bash", str(DETECT_SCRIPT)], capture_output=True, text=True, timeout=10)
    except (OSError, subprocess.TimeoutExpired):
        return {}
    found = {}
    for line in proc.stdout.splitlines():
        if "=" in line:
            key, val = line.strip().split("=", 1)
            found[key] = val
    return found


def plan_smt(args) -> list[tuple[str, str]]:
    """Devuelve [(etiqueta, ids_cpu_csv), ...] de las configuraciones smt a medir."""
    if args.no_smt:
        return []
    if not (BUILD_DIR / SMT).exists():
        print("smt: build/smt no existe (solo compila en Linux) -> se omite smt/smt_ctrl", file=sys.stderr)
        return []
    detected = detect_cpus()
    sibling = args.smt_cpus or detected.get("sibling")
    separate = args.smt_ctrl_cpus or detected.get("separate")
    plan = []
    if sibling:
        plan.append((SMT, sibling))
    else:
        print("smt: no encontre 2 CPUs logicas hermanas (SMT apagado o sin lscpu) -> se omite smt. "
              "Se puede forzar con --smt-cpus A,B", file=sys.stderr)
    if separate:
        plan.append((SMT_CTRL, separate))
    else:
        print("smt_ctrl: no pude elegir 2 nucleos distintos -> se omite. Se puede forzar con --smt-ctrl-cpus A,B", file=sys.stderr)
    return plan


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--runs", type=int, default=5, help="repeticiones por configuracion (default: 5; usar 200+ para la medicion oficial)")
    ap.add_argument("--threads", type=str, default="1,2,4,8", help="cantidades de hilos a probar, separadas por coma (default: 1,2,4,8)")
    ap.add_argument("--width", type=int, default=800)
    ap.add_argument("--height", type=int, default=600)
    ap.add_argument("--spheres", type=int, default=12)
    ap.add_argument("--depth", type=int, default=4)
    ap.add_argument("--out", type=str, default="results/bench.csv", help="ruta del CSV de salida (relativa a proyecto_1/case_4)")
    ap.add_argument("--build", action="store_true", help="correr `make build` antes de medir")
    ap.add_argument("--no-smt", action="store_true", help="no medir smt ni smt_ctrl")
    ap.add_argument("--smt-cpus", type=str, default=None, help="IDs de CPU logica para smt (hermanas), ej. 3,7 (default: autodeteccion)")
    ap.add_argument("--smt-ctrl-cpus", type=str, default=None, help="IDs de CPU logica para smt_ctrl (nucleos distintos), ej. 0,1 (default: autodeteccion)")
    args = ap.parse_args()

    thread_list = [int(t) for t in args.threads.split(",") if t.strip()]
    out_path = CASE4_DIR / args.out
    out_path.parent.mkdir(parents=True, exist_ok=True)

    if args.build:
        print("compilando (make build)...", file=sys.stderr)
        subprocess.run(["make", "build"], cwd=CASE4_DIR, check=True)

    common = [str(args.width), str(args.height), str(args.spheres), str(args.depth)]
    smt_plan = plan_smt(args)

    rows = []  # (engine, threads, run, time_sec)
    total_configs = 1 + len(THREADED_ENGINES) * len(thread_list) + len(smt_plan)
    print(f"{total_configs} configuraciones x {args.runs} corridas = {total_configs * args.runs} ejecuciones totales", file=sys.stderr)

    # sequential: no varia con threads, se corre aparte y se etiqueta threads=1
    seq_bin = BUILD_DIR / SEQUENTIAL
    for run_idx in range(1, args.runs + 1):
        t = run_once(seq_bin, common)
        rows.append((SEQUENTIAL, 1, run_idx, t))
        print(f"  sequential            run {run_idx}/{args.runs}: {t:.6f}s", file=sys.stderr)

    for engine in THREADED_ENGINES:
        binary = BUILD_DIR / engine
        for threads in thread_list:
            for run_idx in range(1, args.runs + 1):
                t = run_once(binary, [*common, str(threads)])
                rows.append((engine, threads, run_idx, t))
                print(f"  {engine:<20} threads={threads:<2} run {run_idx}/{args.runs}: {t:.6f}s", file=sys.stderr)

    smt_bin = BUILD_DIR / SMT
    for label, cpus in smt_plan:
        n_threads = len(cpus.split(","))
        print(f"{label}: CPUs logicas {cpus} ({n_threads} hilos)", file=sys.stderr)
        for run_idx in range(1, args.runs + 1):
            t = run_once(smt_bin, [*common, cpus])
            rows.append((label, n_threads, run_idx, t))
            print(f"  {label:<20} cpus={cpus:<5} run {run_idx}/{args.runs}: {t:.6f}s", file=sys.stderr)

    with out_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["engine", "width", "height", "spheres", "depth", "threads", "run", "time_sec"])
        for engine, threads, run_idx, t in rows:
            writer.writerow([engine, args.width, args.height, args.spheres, args.depth, threads, run_idx, f"{t:.9f}"])

    print(f"\nCSV guardado en {out_path}", file=sys.stderr)

    # Resumen rapido: tiempo promedio por configuracion (no reemplaza el
    # analisis de speedup/eficiencia/escalabilidad, que se hace en un script
    # aparte a partir de este CSV).
    print("\nResumen (promedio de tiempo por configuracion):", file=sys.stderr)
    by_config: dict[tuple[str, int], list[float]] = {}
    for engine, threads, _run_idx, t in rows:
        by_config.setdefault((engine, threads), []).append(t)
    for (engine, threads), times in by_config.items():
        avg = statistics.mean(times)
        label = engine if engine == SEQUENTIAL else f"{engine} (threads={threads})"
        print(f"  {label:<30} promedio {avg:.6f}s  (n={len(times)})", file=sys.stderr)


if __name__ == "__main__":
    main()
