#!/usr/bin/env bash
# Sweep of the fpga backend on the DE1-SoC: runs every filter and size with --validate, appends
# one row per run to a CSV and prints a summary table when it finishes.
# The board must be programmed with fpga/jtag/output_files/vproc_jtag.sof.
#
# Usage (from proyecto_2):
#   make fpga_jtag_bench
#   make fpga_jtag_bench FPGA_SIZES="32 64" FPGA_REPS=3
#
# Optional variables:
#   FPGA_SIZES="32 64 100 128 256 512"   image sides (100 is not a multiple of 8 on purpose)
#   FPGA_FILTERS="gauss sobel"
#   FPGA_REPS=5 FPGA_WARMUP=1
#   OUT=results/fpga/de1soc_<date>.csv   one new file per run by default
#   BIN=build/dlp_bench
set -uo pipefail
cd "$(dirname "$0")/.."

BIN=${BIN:-build/dlp_bench}
SIZES=${FPGA_SIZES:-"32 64 100 128 256 512"}
FILTERS=${FPGA_FILTERS:-"gauss sobel"}
REPS=${FPGA_REPS:-5}
WARMUP=${FPGA_WARMUP:-1}
OUT=${OUT:-results/fpga/de1soc_$(date +%Y%m%d_%H%M).csv}
LOG=build/fpga/jtag_bench.log

if [ ! -x "$BIN" ]; then
    echo "No existe $BIN. Compile primero con: make build" >&2
    exit 1
fi
mkdir -p "$(dirname "$OUT")" "$(dirname "$LOG")"
: > "$LOG"

echo "FPGA (DE1-SoC por JTAG): filtros [$FILTERS], tamanos [$SIZES], $REPS reps + $WARMUP calentamiento"
echo "CSV: $OUT"
echo "Log completo: $LOG"
echo

failures=0
for filter in $FILTERS; do
    for size in $SIZES; do
        printf "  %-6s %4sx%-4s ... " "$filter" "$size" "$size"
        echo "=== $filter ${size}x${size}" >> "$LOG"
        "$BIN" --backend fpga --filter "$filter" --size "$size" \
               --reps "$REPS" --warmup "$WARMUP" --validate --csv "$OUT" \
               --output /dev/null >> "$LOG" 2>&1
        status=$?
        case $status in
            0) echo "ok" ;;
            3) echo "DIFERENCIAS contra seq"; failures=$((failures + 1)) ;;
            *) echo "ERROR ($(grep -h '^Error' "$LOG" | tail -n 1))"; failures=$((failures + 1)) ;;
        esac
    done
done
echo

if [ -s "$OUT" ]; then
    python3 scripts/fpga_table.py "$OUT"
fi

if [ "$failures" -gt 0 ]; then
    echo "$failures corrida(s) con problemas, revise $LOG" >&2
    exit 1
fi
