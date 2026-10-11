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

Los resultados se guardan en results/ht_on/benchmark o results/ht_off/benchmark segun
Hyper-Threading este activo o apagado en esta maquina (se detecta solo; ver ht_mode.py).
Con HT apagado solo hay tantas CPUs logicas como nucleos fisicos, asi que se omiten las
cantidades de hilos que no caben (ej. 8 hilos en 4 CPUs).

Uso basico (desde proyecto_1/case_4):
    python3 scripts/bench.py --runs 5
Las configuraciones se intercalan (una corrida de cada una por ronda, orden aleatorio) para que
la deriva termica o la carga de fondo no sesgue a una sola; --no-interleave vuelve al modo en bloques.
Para la corrida oficial (200+ repeticiones):
    python3 scripts/bench.py --runs 200 --out bench_oficial.csv
"""
import argparse
import csv
import os
import random
import statistics
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ht_mode  # noqa: E402  (deteccion de Hyper-Threading y carpetas de results/)

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
    ap.add_argument("--out", type=str, default="bench.csv",
                    help="CSV de salida. Solo el nombre (ej. bench_oficial.csv): va a results/ht_<on|off>/benchmark/. "
                         "Con una ruta (ej. results/otro/x.csv, relativa a proyecto_1/case_4) se usa tal cual.")
    ap.add_argument("--no-interleave", action="store_true",
                    help="correr cada configuracion en bloque (modo viejo). Por defecto se intercalan: en cada ronda "
                         "se corre una vez cada configuracion, en orden aleatorio")
    ap.add_argument("--live", action="store_true",
                    help="imprimir cada corrida mientras se mide, con una pausa corta despues de cada una "
                         "(el default muestra el detalle al final)")
    ap.add_argument("--pausa", type=float, default=0.05,
                    help="segundos de pausa despues de cada corrida en modo --live (default: 0.05)")
    ap.add_argument("--seed", type=int, default=0, help="semilla del orden aleatorio por ronda (default: 0)")
    ap.add_argument("--build", action="store_true", help="correr `make build` antes de medir")
    ap.add_argument("--no-smt", action="store_true", help="no medir smt ni smt_ctrl")
    ap.add_argument("--smt-cpus", type=str, default=None, help="IDs de CPU logica para smt (hermanas), ej. 3,7 (default: autodeteccion)")
    ap.add_argument("--smt-ctrl-cpus", type=str, default=None, help="IDs de CPU logica para smt_ctrl (nucleos distintos), ej. 0,1 (default: autodeteccion)")
    args = ap.parse_args()

    mode, source = ht_mode.detect_mode()
    thread_list = [int(t) for t in args.threads.split(",") if t.strip()]
    # cmp fija el hilo t a la CPU t: con mas hilos que CPUs logicas no se puede (con HT apagado hay la mitad).
    n_cpus = len(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else (os.cpu_count() or 1)
    dropped = [t for t in thread_list if t > n_cpus]
    if dropped:
        thread_list = [t for t in thread_list if t <= n_cpus]
        print(f"AVISO: esta maquina tiene {n_cpus} CPUs logicas: se omiten {dropped} hilos (usando {thread_list})", file=sys.stderr)
    if Path(args.out).parent == Path("."):
        out_path = ht_mode.subdir("benchmark", mode) / args.out
    else:
        out_path = CASE4_DIR / args.out
        out_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"Hyper-Threading: {mode} (detectado por {source}) -> resultados en {out_path.parent}", file=sys.stderr)

    if args.build:
        print("compilando (make build)...", file=sys.stderr)
        subprocess.run(["make", "build"], cwd=CASE4_DIR, check=True)

    common = [str(args.width), str(args.height), str(args.spheres), str(args.depth)]
    smt_plan = plan_smt(args)

    # Lista de configuraciones: (etiqueta de motor, hilos, binario, argumentos, texto para el log)
    configs = [(SEQUENTIAL, 1, BUILD_DIR / SEQUENTIAL, common, "sequential")]
    for engine in THREADED_ENGINES:
        for threads in thread_list:
            configs.append((engine, threads, BUILD_DIR / engine, [*common, str(threads)],
                            f"{engine} threads={threads}"))
    for label, cpus in smt_plan:
        n_threads = len(cpus.split(","))
        print(f"{label}: CPUs logicas {cpus} ({n_threads} hilos)", file=sys.stderr)
        configs.append((label, n_threads, BUILD_DIR / SMT, [*common, cpus], f"{label} cpus={cpus}"))

    print(f"{len(configs)} configuraciones x {args.runs} corridas = {len(configs) * args.runs} ejecuciones totales", file=sys.stderr)

    rows = []  # (engine, threads, run, time_sec)
    if args.no_interleave:
        # Modo viejo: todas las corridas de una configuracion seguidas. Un cambio de temperatura
        # o de carga de la maquina durante la sesion sesga a una configuracion completa.
        order = [(ci, r) for ci in range(len(configs)) for r in range(1, args.runs + 1)]
        print("modo: configuraciones en bloques (--no-interleave)", file=sys.stderr)
    else:
        # Modo intercalado: en cada ronda se corre UNA vez cada configuracion, en orden aleatorio.
        # Asi la deriva termica o la carga de fondo se reparten por igual entre todas.
        rng = random.Random(args.seed)
        order = []
        for r in range(1, args.runs + 1):
            idx = list(range(len(configs)))
            rng.shuffle(idx)
            order.extend((ci, r) for ci in idx)
        print(f"modo: intercalado, orden aleatorio por ronda (semilla {args.seed})", file=sys.stderr)

    # IMPORTANTE: no imprimir una linea por corrida MIENTRAS se mide. La terminal grafica tarda
    # unos ms en dibujarla justo cuando empieza la siguiente corrida, le quita tiempo de CPU a
    # los workers (sobre todo con HT apagado o con tantos hilos como CPUs) y sesga la medicion.
    # Por eso el detalle de cada corrida se guarda en memoria y se imprime AL FINAL; durante la
    # medicion solo se avisa el progreso cada 5%. Con --live se imprime en vivo, con una pausa por corrida.
    total = len(order)
    step = max(1, total // 20)
    detail = []
    if args.live:
        print(f"modo --live: se imprime cada corrida y se pausa {args.pausa * 1000:.0f} ms despues de cada una "
              f"para que la terminal no interfiera con la siguiente", file=sys.stderr)
    for done, (ci, run_idx) in enumerate(order, start=1):
        engine, threads, binary, bargs, text = configs[ci]
        t = run_once(binary, bargs)
        rows.append((ci, engine, threads, run_idx, t))
        line = f"  {text:<26} run {run_idx}/{args.runs}: {t:.6f}s"
        if args.live:
            print(line, file=sys.stderr, flush=True)
            # pausa para que la terminal termine de dibujar la linea ANTES de la proxima corrida;
            # el tiempo medido lo toma el propio motor, asi que la pausa no cuenta en los resultados
            time.sleep(args.pausa)
        else:
            detail.append(line)
            if done % step == 0 or done == total:
                print(f"  progreso: {done}/{total} ({100 * done // total}%)", file=sys.stderr)

    if detail:
        print("\nDetalle de cada corrida (en el orden en que se ejecutaron):", file=sys.stderr)
        print("\n".join(detail), file=sys.stderr)

    # El CSV queda ordenado por configuracion y numero de corrida (igual que antes).
    rows.sort(key=lambda x: (x[0], x[3]))
    rows = [(e, th, r, t) for _ci, e, th, r, t in rows]

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
