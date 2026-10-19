#!/usr/bin/env bash
#
# sweep_quick.sh
#
# DEFENSE MODE: a short version of the full campaign (scripts/campaign_*.sh)
# that fits in a live defense. It runs the four execution models (CMP, SMT,
# CGMT, FGMT) plus the sequential baseline over the same thread counts as
# the full campaign, but with a single N, fewer steps per run and fewer
# repetitions. The methodology is the same as the full campaign: all runs
# in randomized order, with one warmup run before every measured run.
#
# Why these defaults:
#   N=4096   same N as the paper figures (the full campaign used 15 steps).
#   STEPS=8  each step does the same work as in the full campaign, so ratios
#            between models are preserved; only fewer steps are averaged
#            inside each run (more noise per run, compensated by REPS).
#   REPS=30  the convergence plot of the full campaign shows the 95% CI is
#            already tight at that point.
#
# Usage (from the case_2 root, after `make build`; normally via
# `make sweep-quick`):
#   N=4096 STEPS=8 REPS=30 scripts/sweep_quick.sh
#   MODELS="cgmt" scripts/sweep_quick.sh        # only some models
# MODELS is a space-separated subset of: cmp smt cgmt fgmt (default: all).
# The sequential baseline always runs (it is benchcmp with 1 thread), since
# speedup and efficiency need it.
#   SMT=off scripts/sweep_quick.sh              # label for the SMT-off session
# Writes results/quick/sweep_quick_<SMT>.csv (one row per measured run). It
# does NOT toggle SMT: it measures whatever SMT state the system is in now, and
# SMT=on|off is only the label (default on). Run it once in each state to
# compare them (sweep_quick_summary.py prints the on-vs-off comparison).

set -euo pipefail

N="${N:-4096}"
STEPS="${STEPS:-8}"
REPS="${REPS:-30}"
MODELS="${MODELS:-cmp smt cgmt fgmt}"
# SMT=on|off only labels the output file (it does NOT toggle SMT).
SMT="${SMT:-on}"
case "$SMT" in
    on|off) ;;
    *) echo "SMT debe ser 'on' u 'off' (recibido: '$SMT')" >&2; exit 1 ;;
esac
THREAD_COUNTS=(1 2 4 8 16)
# Same fixed values as the full campaign (chosen from its pilot runs).
CHUNK_SIZE=8
QUANTUM=64

OUT_DIR="results/quick"
OUT_CSV="$OUT_DIR/sweep_quick_${SMT}.csv"
mkdir -p "$OUT_DIR"

for model in $MODELS; do
    case "$model" in
        cmp|smt|cgmt|fgmt) ;;
        *) echo "Modelo desconocido '$model'. Validos: cmp smt cgmt fgmt" >&2; exit 1 ;;
    esac
done

for bin in benchcmp $(for model in $MODELS; do echo "bench$model"; done); do
    if [[ ! -x "build/$bin" ]]; then
        echo "Falta build/$bin: corre 'make build' primero." >&2
        exit 1
    fi
done

# One entry per measured run: "<program>,<workers>". CMP with 1 thread is
# the sequential baseline (benchcmp labels that row "sequential"). When CMP
# is not among the chosen models, the baseline still runs, alone.
ALL_RUNS=()
for model in $MODELS; do
    for workers in "${THREAD_COUNTS[@]}"; do
        for ((i = 0; i < REPS; i++)); do
            ALL_RUNS+=("bench$model,$workers")
        done
    done
done
if [[ " $MODELS " != *" cmp "* ]]; then
    for ((i = 0; i < REPS; i++)); do
        ALL_RUNS+=("benchcmp,1")
    done
fi
mapfile -t ALL_RUNS < <(printf '%s\n' "${ALL_RUNS[@]}" | shuf)

# Common columns for every model. benchcgmt and benchfgmt print an extra
# 5th column (chunk / quantum) that is dropped here so all rows line up.
echo "model,n,steps,workers,time_sec,checksum,ns_per_interaction" > "$OUT_CSV"

# Runs one benchmark once with the right argument list for its model.
# benchcmp/benchsmt take (N steps threads); benchcgmt takes an extra chunk
# size and benchfgmt an extra quantum.
Invoke() {
    local program="$1" workers="$2"
    case "$program" in
        benchcmp|benchsmt) "build/$program" "$N" "$STEPS" "$workers" ;;
        benchcgmt)         "build/$program" "$N" "$STEPS" "$workers" "$CHUNK_SIZE" ;;
        benchfgmt)         "build/$program" "$N" "$STEPS" "$workers" "$QUANTUM" ;;
    esac
}

# Keeps the CSV data row of one run in the common column layout.
NormalizeRow() {
    local program="$1"
    case "$program" in
        benchcgmt|benchfgmt) awk -F, -v OFS=, '{print $1,$2,$3,$4,$6,$7,$8}' ;;
        *)                   cat ;;
    esac
}

TOTAL=${#ALL_RUNS[@]}
COUNT=0
for run in "${ALL_RUNS[@]}"; do
    IFS=',' read -r program workers <<< "$run"
    COUNT=$((COUNT + 1))

    printf '\r[%d/%d] %s workers=%s (N=%s, steps=%s)   ' \
        "$COUNT" "$TOTAL" "$program" "$workers" "$N" "$STEPS"

    Invoke "$program" "$workers" > /dev/null                                  # warmup
    Invoke "$program" "$workers" | tail -n 1 | NormalizeRow "$program" >> "$OUT_CSV"
done
printf '\n'

echo "Listo. Corridas guardadas en $OUT_CSV ($TOTAL corridas medidas)."
