#!/usr/bin/env python3
"""bench.py - corre los motores del ray tracer (case_4) muchas veces y
guarda los tiempos en un CSV, para despues calcular speedup, eficiencia,
escalabilidad e intervalos de confianza.

No incluye `smt`: ese motor solo compila/corre en la maquina Linux fisica
(usa afinidad a CPUs logicas hermanas, que no existe en macOS). Se agrega
aparte cuando se corra ahi.

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

# Motores que toman <width> <height> <spheres> <depth> <threads> [salida.bmp]
THREADED_ENGINES = ["coarseGrained", "fineGrained", "cmp"]
# sequential no tiene parametro de threads: <width> <height> <spheres> <depth> [salida.bmp]
SEQUENTIAL = "sequential"


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
    args = ap.parse_args()

    thread_list = [int(t) for t in args.threads.split(",") if t.strip()]
    out_path = CASE4_DIR / args.out
    out_path.parent.mkdir(parents=True, exist_ok=True)

    if args.build:
        print("compilando (make build)...", file=sys.stderr)
        subprocess.run(["make", "build"], cwd=CASE4_DIR, check=True)

    common = [str(args.width), str(args.height), str(args.spheres), str(args.depth)]

    rows = []  # (engine, threads, run, time_sec)
    total_configs = 1 + len(THREADED_ENGINES) * len(thread_list)
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
