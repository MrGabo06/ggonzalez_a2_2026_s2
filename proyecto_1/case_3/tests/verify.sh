#!/usr/bin/env bash
# Runs every model over a matrix of sizes, step counts and model parameters and
# checks each output mesh against the baseline with build/verify_mesh.
set -euo pipefail

BUILD=${BUILD:-build}
OUT="$BUILD/verify"
mkdir -p "$OUT"

sizes=(3 17 97 256)
step_counts=(1 50)
configs=(
    "model_cmp --threads 1"
    "model_cmp --threads 3"
    "model_cmp --threads 4"
    "model_cmp --threads 8"
    "model_smt --threads 2"
    "model_smt --threads 8"
    "model_fine --threads 1 --quantum 1"
    "model_fine --threads 3 --quantum 1"
    "model_fine --threads 4 --quantum 5"
    "model_coarse --threads 1"
    "model_coarse --threads 4 --cache-kib 1"
    "model_coarse --threads 4 --cache-kib 99999"
)

passed=0
failed=0
for n in "${sizes[@]}"; do
    for steps in "${step_counts[@]}"; do
        args="--N $n --steps $steps --quiet"
        "./$BUILD/baseline_seq" $args --out "$OUT/ref.bin" > /dev/null
        for config in "${configs[@]}"; do
            read -r model params <<< "$config"
            threads=$(sed -n 's/.*--threads \([0-9]*\).*/\1/p' <<< "$params")
            (( threads > n )) && continue
            "./$BUILD/$model" $args $params --out "$OUT/cand.bin" > /dev/null
            if result=$("./$BUILD/verify_mesh" "$OUT/ref.bin" "$OUT/cand.bin"); then
                passed=$((passed + 1))
            else
                failed=$((failed + 1))
                echo "FAIL N=$n steps=$steps $config: $result"
            fi
        done
    done
done

echo "verify: $passed passed, $failed failed"
(( failed == 0 ))
