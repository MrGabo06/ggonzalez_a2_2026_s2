#!/usr/bin/env bash
# Wraps one representative run of each model in perf stat, system-wide and split
# per logical CPU, so the output shows which hardware threads did the work and
# how often each stalled on the memory hierarchy.
# Raw perf output goes to data/perf/<smt>/<model>_N<n>_T<t>.csv (perf -x, format).
#
# Environment overrides:
#   NS="128 1024"   one cache-resident and one DRAM-bound mesh
#   WORK=3e8
#   REPEAT=5        perf stat -r: counts are averaged over this many runs
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD=${BUILD:-build}
NS=${NS:-128 1024}
WORK=${WORK:-3e8}
REPEAT=${REPEAT:-5}

(( $(cat /proc/sys/kernel/perf_event_paranoid) <= 0 )) ||
    { echo "per-CPU counters need perf_event_paranoid <= 0: run 'sudo bench/setup_env.sh apply'" >&2; exit 1; }

events=cycles,instructions,cache-references,cache-misses,L1-dcache-load-misses,LLC-load-misses
events+=,cycle_activity.stalls_total,cycle_activity.stalls_l2_miss,cycle_activity.stalls_l3_miss
events+=,context-switches,cpu-migrations

configs=(
    "baseline_seq"
    "model_cmp --threads 1"
    "model_cmp --threads 4"
    "model_smt --threads 2"
    "model_smt --threads 8"
    "model_fine --threads 4 --quantum 1"
    "model_coarse --threads 4"
)

smt_state=$([[ $(cat /sys/devices/system/cpu/smt/active) == 1 ]] && echo on || echo off)
out_dir="data/perf/smt$smt_state"
mkdir -p "$out_dir"

for n in $NS; do
    steps=$(awk -v w="$WORK" -v n="$n" 'BEGIN { s = int(w / (n * n) + 0.5); print (s < 1 ? 1 : s) }')
    for config in "${configs[@]}"; do
        read -r model params <<< "$config"
        threads=$(sed -n 's/.*--threads \([0-9]*\).*/\1/p' <<< "$params")
        out="$out_dir/${model}_N${n}_T${threads:-1}.csv"
        # shellcheck disable=SC2086
        perf stat -a -A -x, -r "$REPEAT" -e "$events" -o "$out" -- \
            "./$BUILD/$model" --N "$n" --steps "$steps" $params --quiet > /dev/null
        echo "wrote $out" >&2
    done
done
