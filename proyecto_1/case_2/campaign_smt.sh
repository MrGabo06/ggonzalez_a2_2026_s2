#!/usr/bin/env bash
#
# campaign_smt.sh
#
# Pilot measurement campaign for the SMT execution model. Runs ./benchsmt
# across a grid of (N, threadCount) in randomized order, with one warmup
# run before each measured repetition, and appends every repetition to a
# CSV file. Same grid and methodology as campaign_cmp.sh; only the binary
# under test changes.

set -euo pipefail

declare -A STEPS_FOR_N=(
    [1024]=250
    [2048]=60
    [4096]=15
    [8192]=4
)
REPETITIONS=200
OUTPUT_CSV="final_smt.csv"

N_VALUES=(1024 2048 4096 8192)
THREAD_COUNTS=(1 2 4 8 16)
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
    ./benchsmt "$n" "$steps" "$threads" > /dev/null

    echo "[$COUNT/$TOTAL] N=$n threads=$threads steps=$steps - midiendo..."
    ./benchsmt "$n" "$steps" "$threads" | tail -n 1 >> "$OUTPUT_CSV"
done

echo "Done. Results in $OUTPUT_CSV"