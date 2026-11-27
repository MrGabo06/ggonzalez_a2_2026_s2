#!/usr/bin/env bash
# Perfilado de F4: confirma con contadores de hardware POR QUE cada backend rinde como rinde.
#   CPU: perf stat (ciclos, instrucciones, fallos de cache) para los backends de CPU
#   GPU: Nsight Compute (ncu) para los kernels de CUDA
#
# Uso (desde proyecto_2, con el binario ya compilado):
#   make profile            (detecta el hardware, compila, perfila y escribe el resumen)
#   make profile_cpu        solo perf
#   make profile_gpu        solo Nsight Compute
#   o a mano: scripts/run_profile.sh [cpu|gpu|all]
#
# Salida en results/perfilado/ (un conjunto por maquina):
#   <maquina>_perf.csv        contadores de perf por backend, filtro y tamano
#   <maquina>_ncu/*.txt       reporte completo de ncu por kernel
#   <maquina>_perfilado.md    tablas resumen (las genera scripts/profile_report.py)
#
# Variables opcionales:
#   PROF_SIZES="1024 4096"     tamanos para perf (cabe en cache / no cabe en cache)
#   PROF_GPU_SIZE=4096         tamano para ncu
#   PROF_BACKENDS="seq simd"   backends de CPU a perfilar (por defecto todos)
#   PROF_REPS=5 PROF_WARMUP=1  repeticiones medidas y de calentamiento
set -uo pipefail
cd "$(dirname "$0")/.."

MODE=${1:-all}
BIN=build/dlp_bench
HOST=$(hostname)
OUTDIR=results/perfilado
PERF_CSV=$OUTDIR/${HOST}_perf.csv
NCU_DIR=$OUTDIR/${HOST}_ncu
SIZES=${PROF_SIZES:-"1024 4096"}
GPU_SIZE=${PROF_GPU_SIZE:-4096}
CPU_BACKENDS=${PROF_BACKENDS:-"seq seq_sep simd simd_sep simd512 simd_sep512"}
FILTERS=${PROF_FILTERS:-"gauss sobel"}
REPS=${PROF_REPS:-5}
WARMUP=${PROF_WARMUP:-1}
GPU_BACKENDS=${PROF_GPU_BACKENDS:-"cuda cuda_tiled"}

# Con LC_ALL=C las herramientas escriben los numeros igual en cualquier idioma
export LC_ALL=C

if [ ! -x "$BIN" ]; then
    echo "No existe $BIN. Use: make profile (compila y perfila)" >&2
    exit 1
fi
mkdir -p "$OUTDIR"

# ---------------------------------------------------------------- CPU (perf)
profile_cpu() {
    if ! command -v perf >/dev/null 2>&1; then
        echo "CPU omitido: no hay 'perf'. En Ubuntu: sudo apt install linux-tools-common linux-tools-\$(uname -r)"
        return
    fi
    local paranoid
    paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo 99)
    if [ "$paranoid" -gt 1 ]; then
        echo "perf_event_paranoid=$paranoid bloquea los contadores de CPU (hace falta <= 1)."
        echo "Cambio temporal (se revierte al reiniciar): sudo sysctl -w kernel.perf_event_paranoid=1"
        local ans=n
        if [ -t 0 ]; then read -r -p "Ejecutarlo ahora? [s/N] " ans; fi
        case "$ans" in
            s|S|y|Y) sudo sysctl -w kernel.perf_event_paranoid=1 >/dev/null ;;
            *) echo "CPU omitido."; return ;;
        esac
    fi

    echo "backend,filter,size,cycles,instructions,cache_references,cache_misses,l1d_load_misses,median_ms" > "$PERF_CSV"
    local tmp_perf tmp_csv
    tmp_perf=$(mktemp); tmp_csv=$(mktemp)
    for backend in $CPU_BACKENDS; do
        for filter in $FILTERS; do
            for size in $SIZES; do
                rm -f "$tmp_csv"
                if ! err=$(perf stat -x, -o "$tmp_perf" \
                        -e cycles,instructions,cache-references,cache-misses,L1-dcache-load-misses \
                        "$BIN" --backend "$backend" --filter "$filter" --size "$size" \
                        --reps "$REPS" --warmup "$WARMUP" --csv "$tmp_csv" \
                        --output /dev/null 2>&1 >/dev/null); then
                    echo "OMITIDO $backend ($(echo "$err" | head -n 1))"
                    break 2
                fi
                # Una linea por evento: valor,unidad,evento,... (se quita :u, cpu_core/ etc.)
                local row
                row=$(awk -F, '
                    /^#/ || NF < 3 { next }
                    { ev = $3; gsub(/:.*$/, "", ev); gsub(/cpu_(core|atom)\//, "", ev); gsub(/\/$/, "", ev)
                      v = ($1 ~ /^[0-9]+$/) ? $1 : ""
                      if (ev == "cycles") c = v
                      else if (ev == "instructions") i = v
                      else if (ev == "cache-references") r = v
                      else if (ev == "cache-misses") m = v
                      else if (ev == "L1-dcache-load-misses") l = v }
                    END { print c "," i "," r "," m "," l }' "$tmp_perf")
                local median
                median=$(awk -F, 'NR == 2 { print $7 }' "$tmp_csv" 2>/dev/null)
                echo "$backend,$filter,$size,$row,$median" >> "$PERF_CSV"
                echo "ok      perf $backend $filter ${size}x${size}"
            done
        done
    done
    rm -f "$tmp_perf" "$tmp_csv"
    echo "perf: $(($(wc -l < "$PERF_CSV") - 1)) filas en $PERF_CSV"
}

# ------------------------------------------------------ GPU (Nsight Compute)
profile_gpu() {
    if ! command -v ncu >/dev/null 2>&1; then
        echo "GPU omitido: no hay 'ncu' (Nsight Compute viene con el CUDA Toolkit)."
        return
    fi
    if ! command -v nvidia-smi >/dev/null 2>&1 || ! nvidia-smi -L >/dev/null 2>&1; then
        echo "GPU omitido: no hay GPU NVIDIA visible."
        return
    fi
    local ncu_bin
    ncu_bin=$(command -v ncu)
    mkdir -p "$NCU_DIR"
    local prefix=""   # se vuelve "sudo env LC_ALL=C" si ncu pide permisos
    for backend in $GPU_BACKENDS; do
        for filter in $FILTERS; do
            local out="$NCU_DIR/${backend}_${filter}_${GPU_SIZE}.txt"
            local sections="--section SpeedOfLight --section MemoryWorkloadAnalysis --section Occupancy --section LaunchStats --section WarpStateStats --section InstructionStats --section ComputeWorkloadAnalysis"
            # --launch-count 1: perfila solo el primer kernel (basta con 1 corrida)
            # shellcheck disable=SC2086
            $prefix "$ncu_bin" --launch-count 1 $sections "$BIN" --backend "$backend" \
                --filter "$filter" --size "$GPU_SIZE" --reps 1 --warmup 0 \
                --output /dev/null > "$out" 2>&1
            if grep -q "ERR_NVGPUCTRPERM" "$out" && [ -z "$prefix" ]; then
                echo "ncu necesita permisos de administrador para leer los contadores de la GPU; usando sudo."
                prefix="sudo env LC_ALL=C"
                # shellcheck disable=SC2086
                $prefix "$ncu_bin" --launch-count 1 $sections "$BIN" --backend "$backend" \
                    --filter "$filter" --size "$GPU_SIZE" --reps 1 --warmup 0 \
                    --output /dev/null > "$out" 2>&1
            fi
            if grep -q "Section: GPU Speed Of Light" "$out"; then
                echo "ok      ncu  $backend $filter ${GPU_SIZE}x${GPU_SIZE}"
            else
                echo "OMITIDO $backend $filter ($(grep -m1 -E 'ERR|Error|error' "$out" || echo 'ncu no perfilo ningun kernel'))"
                rm -f "$out"
                break 2
            fi
        done
    done
}

case "$MODE" in
    cpu) profile_cpu ;;
    gpu) profile_gpu ;;
    all) profile_cpu; profile_gpu ;;
    *) echo "Uso: $0 [cpu|gpu|all]" >&2; exit 1 ;;
esac

python3 scripts/profile_report.py "$HOST"
