#!/usr/bin/env bash
#
# perf_smt_thread.sh
#
# Perfilado por hilo/CPU logica de benchsmt, pegandose al PID del proceso
# ya corriendo (perf stat -p PID --per-thread) en vez de monitorear todo
# el sistema (-a), que captura todos los demas procesos de la maquina y
# produce una tabla enorme e inutil. Mismo problema que documento el
# companero del caso 1 en su Makefile.
#
# Uso: scripts/perf_smt_thread.sh [N] [STEPS] [THREADS] [archivo_salida]

set -euo pipefail

N="${1:-4096}"
STEPS="${2:-200}"
THREADS="${3:-8}"
OUT="${4:-results/perf/perf_smt_thread.txt}"

mkdir -p "$(dirname "$OUT")"

build/benchsmt "$N" "$STEPS" "$THREADS" > /dev/null &
BENCHPID=$!

perf stat -p "$BENCHPID" --per-thread -e cycles,instructions,context-switches,cpu-migrations \
    2> "$OUT" &
PERFPID=$!

# Espera a que termine el benchmark, luego le manda SIGINT a perf (igual
# que Ctrl-C) para que imprima las estadisticas finales y cierre.
wait "$BENCHPID"
kill -INT "$PERFPID" 2>/dev/null || true
wait "$PERFPID" 2>/dev/null || true

echo "Guardado en $OUT"
