#!/usr/bin/env bash
#
# campaign_fgmt.sh
#
# Pilot measurement campaign for the FGMT execution model. Runs ./benchfgmt
# across a grid of (N, fiberCount, quantumBodies) in randomized order, with
# one warmup run before each measured repetition, and appends every
# repetition to a CSV file. Same methodology as campaign_cmp.sh,
# campaign_smt.sh and campaign_cgmt.sh, using fiberCount/quantumBodies in
# place of threads/chunkSize.
#
# NOTE: pilot-scale grid (reduced repetitions and quantum values), same as
# campaign_cgmt.sh. Restore REPETITIONS=20 and the full QUANTUMS list for
# the final measurement campaign.

set -euo pipefail

declare -A STEPS_FOR_N=(
    [1024]=250
    [2048]=60
    [4096]=15
    [8192]=4
)
REPETITIONS=3
OUTPUT_CSV="campaign_fgmt.csv"

N_VALUES=(1024 2048 4096 8192)
FIBER_COUNTS=(1 2 4 8 16)
QUANTUMS=(8 64)

CONFIGS=()
for n in "${N_VALUES[@]}"; do
    for fibers in "${FIBER_COUNTS[@]}"; do
        for quantum in "${QUANTUMS[@]}"; do
            CONFIGS+=("$n,$fibers,$quantum")
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

echo "model,n,steps,workers,quantum,time_sec,checksum,ns_per_interaction" > "$OUTPUT_CSV"

TOTAL=${#ALL_RUNS[@]}
COUNT=0

for config in "${ALL_RUNS[@]}"; do
    IFS=',' read -r n fibers quantum <<< "$config"
    COUNT=$((COUNT + 1))

    steps="${STEPS_FOR_N[$n]}"

    echo "[$COUNT/$TOTAL] N=$n fibers=$fibers quantum=$quantum steps=$steps - calentando..."
    ./benchfgmt "$n" "$steps" "$fibers" "$quantum" > /dev/null

    echo "[$COUNT/$TOTAL] N=$n fibers=$fibers quantum=$quantum steps=$steps - midiendo..."
    ./benchfgmt "$n" "$steps" "$fibers" "$quantum" | tail -n 1 >> "$OUTPUT_CSV"
done

echo "Done. Results in $OUTPUT_CSV"
