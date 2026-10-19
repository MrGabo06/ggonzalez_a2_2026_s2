#!/usr/bin/env bash
#
# campaign_smt.sh
#
# Pilot measurement campaign for the SMT execution model. Runs build/benchsmt
# across a grid of (N, threadCount) in randomized order, with one warmup
# run before each measured repetition, and appends every repetition to a
# CSV file. Same grid and methodology as campaign_cmp.sh; only the binary
# under test changes.
#
# Assumes it is invoked from the case_2 root.
#
# SMT=on|off (default on) only labels the output file (final_smt_on.csv or
# final_smt_off.csv). It does NOT toggle SMT: set SMT in the system first,
# then run this with the matching label.
#
# WORKERS="8 16" (optional) re-measures only those thread counts, for all N,
# and writes results/raw/final_smt_<SMT>_rerun.csv so the existing full
# campaign file is NOT overwritten. Use scripts/merge_smt_rerun.py to compare
# and, if wanted, merge the rerun into the full file.

set -euo pipefail

SMT="${SMT:-on}"
case "$SMT" in
    on|off) ;;
    *) echo "SMT debe ser 'on' u 'off' (recibido: '$SMT')" >&2; exit 1 ;;
esac

declare -A STEPS_FOR_N=(
    [1024]=250
    [2048]=60
    [4096]=15
    [8192]=4
)
REPETITIONS=200
mkdir -p results/raw
OUTPUT_CSV="results/raw/final_smt_${SMT}.csv"

N_VALUES=(1024 2048 4096 8192)
THREAD_COUNTS=(1 2 4 8 16)
if [[ -n "${WORKERS:-}" ]]; then
    read -r -a THREAD_COUNTS <<< "$WORKERS"
    for t in "${THREAD_COUNTS[@]}"; do
        [[ "$t" =~ ^[0-9]+$ ]] || { echo "WORKERS debe ser una lista de enteros (recibido: '$WORKERS')" >&2; exit 1; }
    done
    OUTPUT_CSV="results/raw/final_smt_${SMT}_rerun.csv"
fi
CONFIGS=()
for n in "${N_VALUES[@]}"; do
    for threads in "${THREAD_COUNTS[@]}"; do
        CONFIGS+=("$n,$threads")
    done
done
ALL_RUNS=()
for config in "${CONFIGS[@]}"; do
    for ((i = 0; i < REPETITIONS; i++)); do
        ALL_RUNS+=("$config")
    done
done

mapfile -t ALL_RUNS < <(printf '%s\n' "${ALL_RUNS[@]}" | shuf)
echo "model,n,steps,workers,time_sec,checksum,ns_per_interaction" > "$OUTPUT_CSV"

TOTAL=${#ALL_RUNS[@]}
COUNT=0

for config in "${ALL_RUNS[@]}"; do
    IFS=',' read -r n threads <<< "$config"
    COUNT=$((COUNT + 1))

    steps="${STEPS_FOR_N[$n]}"

    echo "[$COUNT/$TOTAL] N=$n threads=$threads steps=$steps - calentando..."
    build/benchsmt "$n" "$steps" "$threads" > /dev/null

    echo "[$COUNT/$TOTAL] N=$n threads=$threads steps=$steps - midiendo..."
    build/benchsmt "$n" "$steps" "$threads" | tail -n 1 >> "$OUTPUT_CSV"
done

echo "Done. Results in $OUTPUT_CSV"
