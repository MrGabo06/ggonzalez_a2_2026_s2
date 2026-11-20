#!/usr/bin/env bash
# Barrido de tamaños para F4: corre cada backend con cada filtro y tamaño y agrega una
# fila por corrida a un CSV. Cada máquina genera su propio archivo en results/.
#
# Uso (desde proyecto_2, con el binario ya compilado con las opciones de esa máquina):
#   make report            (detecta el hardware, compila, mide y genera graficas)
#   o a mano: make clean && make build SIMD=1 AVX512=1 CUDA=1 -j
#   scripts/run_bench.sh
#
# Variables opcionales:
#   SIZES="512 1024 2048 4096"   lados de la imagen cuadrada
#   BACKENDS="seq simd cuda"     backends a medir (por defecto todos)
#   REPS=10 WARMUP=3             repeticiones medidas y de calentamiento
#   OUT=results/mi_maquina.csv   archivo de salida
set -uo pipefail
cd "$(dirname "$0")/.."

BIN=build/dlp_bench
SIZES=${SIZES:-"512 1024 2048 4096"}
BACKENDS=${BACKENDS:-"seq seq_sep simd simd_sep simd512 simd_sep512 cuda cuda_tiled"}
FILTERS=${FILTERS:-"gauss sobel"}
REPS=${REPS:-10}
WARMUP=${WARMUP:-3}
OUT=${OUT:-results/$(hostname)_$(date +%Y%m%d_%H%M).csv}

if [ ! -x "$BIN" ]; then
    echo "No existe $BIN. Compile primero con: make build SIMD=1 AVX512=1 CUDA=1 -j (o use make report)" >&2
    exit 1
fi
mkdir -p "$(dirname "$OUT")"
echo "Resultados en $OUT"

for backend in $BACKENDS; do
    for filter in $FILTERS; do
        for size in $SIZES; do
            # --validate deja en el CSV el error contra seq (diferencias numéricas)
            if err=$("$BIN" --backend "$backend" --filter "$filter" --size "$size" \
                     --reps "$REPS" --warmup "$WARMUP" --validate --csv "$OUT" \
                     --output /dev/null 2>&1 >/dev/null); then
                echo "ok      $backend $filter ${size}x${size}"
            else
                # Backend no compilado o sin hardware: se omite y se muestra el motivo
                echo "OMITIDO $backend ($(echo "$err" | head -n 1))"
                break 2
            fi
        done
    done
done
echo "Listo: $(($(wc -l < "$OUT") - 1)) filas en $OUT"
