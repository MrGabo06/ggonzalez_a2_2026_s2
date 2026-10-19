#!/usr/bin/env bash
#
# sample_cpu_affinity.sh
#
# Alternativa a "perf stat --per-thread" cuando el motor recrea hilos muy
# seguido (perf no logra engancharlos de forma estable). Muestrea con
# "ps -L" cada pocos milisegundos mientras corre el benchmark, registrando
# en que CPU logica (psr) esta cada hilo (tid) en cada instante. Con eso
# se puede reconstruir un mapa de "trabajo por CPU logica" similar al que
# usa el companero del caso 3 (heatmap), sin depender de que perf
# encuentre los hilos a tiempo.
#
# Uso: scripts/sample_cpu_affinity.sh [N] [STEPS] [THREADS] [archivo_salida]

set -euo pipefail

N="${1:-4096}"
STEPS="${2:-200}"
THREADS="${3:-8}"
OUT="${4:-results/perf/cpu_affinity_sample.csv}"
SAMPLE_INTERVAL="0.01"

mkdir -p "$(dirname "$OUT")"

build/benchsmt "$N" "$STEPS" "$THREADS" > /dev/null &
PID=$!

echo "timestamp_ns,tid,psr,pcpu" > "$OUT"
while kill -0 "$PID" 2>/dev/null; do
    ts=$(date +%s%N)
    ps -L -o tid,psr,pcpu --no-headers -p "$PID" 2>/dev/null \
        | awk -v ts="$ts" '{print ts","$1","$2","$3}' >> "$OUT"
    sleep "$SAMPLE_INTERVAL"
done

echo "Guardado en $OUT"
echo "Filas capturadas: $(($(wc -l < "$OUT") - 1))"
