#!/usr/bin/env bash
# Runs every configuration RUNS times in one shuffled order, so thermal drift
# spreads over all configurations instead of biasing whichever ran last.
# Appends one row per run to data/bench_smt<on|off>_<timestamp>.csv.
#
# Environment overrides:
#   RUNS=200            repetitions per configuration
#   NS="128 512 1024 2048"
#   WORK=3e8            N^2 * steps per run, constant across N (~0.5 s baseline)
#   MODELS="baseline_seq model_cmp model_smt model_fine model_coarse"
#   SEED=42             shuffle seed
#   COOL_C=75           wait before a run until the package is below this
#   FORCE=1             measure even if setup_env.sh check fails
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD=${BUILD:-build}
RUNS=${RUNS:-200}
NS=${NS:-128 512 1024 2048}
WORK=${WORK:-3e8}
MODELS=${MODELS:-baseline_seq model_cmp model_smt model_fine model_coarse}
SEED=${SEED:-42}
COOL_C=${COOL_C:-75}

if ! bench/setup_env.sh check; then
    [[ ${FORCE:-0} == 1 ]] || { echo "run 'sudo bench/setup_env.sh apply' first, or FORCE=1" >&2; exit 1; }
    echo "FORCE=1: measuring in a noisy environment" >&2
fi

declare -A params=(
    [baseline_seq]=""
    [model_cmp]="--threads 1|--threads 2|--threads 4"
    [model_smt]="--threads 2|--threads 4|--threads 8"
    [model_fine]="--threads 4 --quantum 1"
    [model_coarse]="--threads 4"
)

smt_state=$([[ $(cat /sys/devices/system/cpu/smt/active) == 1 ]] && echo on || echo off)
mkdir -p data
out="data/bench_smt${smt_state}_$(date +%Y%m%d-%H%M%S).csv"
jobs=$(mktemp)
trap 'rm -f "$jobs"' EXIT

for n in $NS; do
    steps=$(awk -v w="$WORK" -v n="$n" 'BEGIN { s = int(w / (n * n) + 0.5); print (s < 1 ? 1 : s) }')
    for model in $MODELS; do
        IFS='|' read -ra variants <<< "${params[$model]:-}"
        (( ${#variants[@]} )) || variants=("")
        for variant in "${variants[@]}"; do
            for ((run = 1; run <= RUNS; run++)); do
                echo "$model;$n;$steps;$variant;$run"
            done
        done
    done
done | shuf --random-source=<(yes "$SEED") > "$jobs"

thermal_zone=$(grep -l x86_pkg_temp /sys/class/thermal/thermal_zone*/type | head -1)
thermal_zone=${thermal_zone%/type}
throttle_file=/sys/devices/system/cpu/cpu0/thermal_throttle/package_throttle_count
pkg_temp_c() { echo $(( $(cat "$thermal_zone/temp") / 1000 )); }
mean_freq_mhz() { awk '{ s += $1 } END { printf "%.0f", s / NR / 1000 }' /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq; }

echo "timestamp,smt,model,N,threads,steps,quantum,run,time_s,checksum,switches,freq_mhz,temp_c,throttled" > "$out"
total=$(wc -l < "$jobs")
done_count=0
while IFS=';' read -r model n steps variant run; do
    while (( $(pkg_temp_c) >= COOL_C )); do sleep 1; done
    temp=$(pkg_temp_c)
    throttle_before=$(cat "$throttle_file")
    # shellcheck disable=SC2086
    line=$("./$BUILD/$model" --N "$n" --steps "$steps" $variant --quiet)
    freq=$(mean_freq_mhz)
    throttled=$(( $(cat "$throttle_file") > throttle_before ? 1 : 0 ))
    quantum=$(sed -n 's/.*--quantum \([0-9]*\).*/\1/p' <<< "$variant")
    IFS=, read -r name _ threads _ time_s checksum switches <<< "$line"
    echo "$(date +%s),$smt_state,$name,$n,$threads,$steps,${quantum:-NA},$run,$time_s,$checksum,$switches,$freq,$temp,$throttled" >> "$out"
    done_count=$((done_count + 1))
    (( done_count % 100 == 0 || done_count == total )) && echo "[$done_count/$total] $out" >&2
done < "$jobs"

echo "wrote $out" >&2
