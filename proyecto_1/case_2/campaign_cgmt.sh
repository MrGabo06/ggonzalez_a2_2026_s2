#!/usr/bin/env bash
#
# campaign_cgmt.sh
#
# Pilot measurement campaign for the CGMT execution model. Runs ./benchcgmt
# across a grid of (N, threadCount, chunkSize) in randomized order, with one
# warmup run before each measured repetition, and appends every repetition
# to a CSV file. Same methodology as campaign_cmp.sh and campaign_smt.sh,
# extended with the chunk-size dimension that CGMT needs.

set -euo pipefail

declare -A STEPS_FOR_N=(
    [1024]=250
    [2048]=60
    [4096]=15
    [8192]=4
)
REPETITIONS=20
OUTPUT_CSV="campaign_cgmt.csv"

N_VALUES=(1024 2048 4096 8192)
THREAD_COUNTS=(1 2 4 8 16)
CHUNK_SIZES=(1 8 32 128)

CONFIGS=()
for n in "${N_VALUES[@]}"; do
    for threads in "${THREAD_COUNTS[@]}"; do
        for chunk in "${CHUNK_SIZES[@]}"; do
            CONFIGS+=("$n,$threads,$chunk")
        done
    done
done

ALL_RUNS=()
for config in "${CONFIGS[@]}"; do
    for ((i = 0; i < REPETITIONS; i++)); do
        ALL_RUNS+=("$config")
    done
done

mapfile -t ALL_RUNS < <(printf '%s\n' "${ALL_RUNS[@]}" | shuf)

echo "model,n,steps,workers,chunk,time_sec,checksum,ns_per_interaction" > "$OUTPUT_CSV"

TOTAL=${#ALL_RUNS[@]}
COUNT=0

for config in "${ALL_RUNS[@]}"; do
    IFS=',' read -r n threads chunk <<< "$config"
    COUNT=$((COUNT + 1))

    steps="${STEPS_FOR_N[$n]}"

    echo "[$COUNT/$TOTAL] N=$n threads=$threads chunk=$chunk steps=$steps - calentando..."
    ./benchcgmt "$n" "$steps" "$threads" "$chunk" > /dev/null

    echo "[$COUNT/$TOTAL] N=$n threads=$threads chunk=$chunk steps=$steps - midiendo..."
    ./benchcgmt "$n" "$steps" "$threads" "$chunk" | tail -n 1 >> "$OUTPUT_CSV"
done

echo "Done. Results in $OUTPUT_CSV"