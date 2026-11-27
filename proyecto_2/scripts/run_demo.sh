#!/usr/bin/env bash
# Runs one family of backends over a photo for `make demo`: converts the photo to PGM, runs every
# compiled backend of the family with both filters and --validate, and renders the final images.
# Outputs go to examples/output/<photo name>/, which git ignores.
#
# Usage (from proyecto_2, normally through make):
#   scripts/run_demo.sh prepare | seq | simd | cuda | fpga | images
#
# Variables (set by the Makefile):
#   DEMO_IMAGE=examples/photo.jpg   photo to filter (default: the only image in examples/)
#   DEMO_SIZE=512                   side of the square grayscale crop given to every backend
#   DEMO_REPS=1 DEMO_WARMUP=0       repetitions and warm-up runs of dlp_bench
#   DEMO_FILTERS="gauss sobel"
#   DEMO_OPTIONAL=1                 skip a family that is not compiled instead of failing
#   BIN=build/dlp_bench
set -uo pipefail
cd "$(dirname "$0")/.."

BIN=${BIN:-build/dlp_bench}
DEMO_SIZE=${DEMO_SIZE:-512}
DEMO_REPS=${DEMO_REPS:-1}
DEMO_WARMUP=${DEMO_WARMUP:-0}
DEMO_FILTERS=${DEMO_FILTERS:-"gauss sobel"}
DEMO_OPTIONAL=${DEMO_OPTIONAL:-0}

fail() {
    echo "$*" >&2
    exit 1
}

resolve_image() {
    if [ -n "${DEMO_IMAGE:-}" ]; then
        [ -f "$DEMO_IMAGE" ] || fail "No existe la imagen $DEMO_IMAGE"
        echo "$DEMO_IMAGE"
        return
    fi
    local found
    mapfile -t found < <(find examples -maxdepth 1 -type f \
        \( -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.png' -o -iname '*.bmp' -o -iname '*.pgm' \) | sort)
    case ${#found[@]} in
        0) fail "No hay imagenes en examples/. Copie una foto ahi o use make demo DEMO_IMAGE=<ruta>" ;;
        1) echo "${found[0]}" ;;
        *) fail "Hay varias imagenes en examples/ (${found[*]}). Elija una con DEMO_IMAGE=<ruta>" ;;
    esac
}

image=$(resolve_image) || exit 1
name=$(basename "${image%.*}")
demo_dir=examples/output/$name
input_pgm=$demo_dir/input.pgm

input_stamp=$demo_dir/.input-stamp
current_stamp="$image|$DEMO_SIZE"

prepare_input() {
    mkdir -p "$demo_dir"
    rm -rf "$demo_dir/png" "$demo_dir/montage.png" "$demo_dir"/*.pgm
    python3 -I scripts/demo_images.py prepare "$image" "$input_pgm" "$DEMO_SIZE" || return 1
    echo "$current_stamp" > "$input_stamp"
}

# Outputs of another photo or size must never share a folder with new ones, so a changed input
# starts the folder over instead of reusing the old input.pgm.
ensure_input() {
    if [ -f "$input_pgm" ] && [ "$(cat "$input_stamp" 2>/dev/null)" = "$current_stamp" ] \
       && [ ! "$image" -nt "$input_pgm" ]; then
        return
    fi
    echo "  entrada nueva o distinta: se regenera $demo_dir"
    prepare_input || fail "No se pudo convertir $image"
}

compiled_backends() {
    "$BIN" --help 2>&1 | sed -n 's/^Backends compilados: //p'
}

backends_of_family() {
    case $1 in
        seq)  echo "seq seq_sep" ;;
        simd) echo "simd simd_sep simd512 simd_sep512" ;;
        cuda) echo "cuda cuda_tiled" ;;
        fpga) echo "fpga" ;;
        *)    fail "Familia desconocida: $1" ;;
    esac
}

build_hint() {
    case $1 in
        simd) echo "make clean && make build SIMD=1 AVX512=1 -j (AVX512=1 solo en CPU con AVX-512)" ;;
        cuda) echo "make clean && make build CUDA=1 -j (requiere nvcc y una GPU NVIDIA)" ;;
        *)    echo "make build" ;;
    esac
}

format_ms() {
    LC_ALL=C awk -v value="$1" 'BEGIN { printf "%10.3f ms", value }'
}

run_one() {
    local backend=$1 filter=$2 log status total kernel
    printf "  %-6s %-12s " "$filter" "$backend"
    log=$("$BIN" --backend "$backend" --filter "$filter" --input "$input_pgm" \
          --reps "$DEMO_REPS" --warmup "$DEMO_WARMUP" --validate \
          --output "$demo_dir/${filter}_${backend}.pgm" 2>&1)
    status=$?
    case $status in
        0) ;;
        3) echo "DIFERENCIAS contra seq"; return 1 ;;
        *) echo "ERROR: $(grep -h '^Error' <<<"$log" | tail -n 1)"; echo "$log" | tail -n 2 >&2; return 1 ;;
    esac
    total=$(sed -n 's/^summary,.*median=\([^,]*\),.*/\1/p' <<<"$log" | tail -n 1)
    kernel=$(sed -n 's/^kernel,.*median=\([^,]*\),.*/\1/p' <<<"$log" | tail -n 1)
    printf "total %s" "$(format_ms "$total")"
    [ -n "$kernel" ] && printf "   kernel %s" "$(format_ms "$kernel")"
    echo "   validacion ok"
}

run_family() {
    local family=$1 compiled backend filter present=() missing=() failures=0
    [ -x "$BIN" ] || fail "No existe $BIN. Compile primero con: make build"
    compiled=" $(compiled_backends) "
    for backend in $(backends_of_family "$family"); do
        case $compiled in
            *" $backend "*) present+=("$backend") ;;
            *) missing+=("$backend") ;;
        esac
    done
    if [ ${#present[@]} -eq 0 ]; then
        if [ "$DEMO_OPTIONAL" = 1 ]; then
            echo "[$family] omitido: no esta compilado. Para incluirlo: $(build_hint "$family")"
            return 0
        fi
        fail "[$family] no esta compilado en $BIN. Compile con: $(build_hint "$family")"
    fi
    ensure_input
    echo "[$family] $image -> ${DEMO_SIZE}x${DEMO_SIZE}, $DEMO_REPS rep(s) + $DEMO_WARMUP de calentamiento"
    [ ${#missing[@]} -gt 0 ] && echo "  no compilados, se omiten: ${missing[*]}"
    for filter in $DEMO_FILTERS; do
        for backend in "${present[@]}"; do
            run_one "$backend" "$filter" || failures=$((failures + 1))
        done
    done
    [ "$failures" -eq 0 ]
}

case ${1:-} in
    prepare)
        echo "[prepare] $image"
        prepare_input || fail "No se pudo convertir $image"
        ;;
    seq|simd|cuda|fpga)
        run_family "$1"
        ;;
    images)
        echo "[images] $demo_dir"
        python3 -I scripts/demo_images.py render "$demo_dir"
        ;;
    *)
        fail "Uso: scripts/run_demo.sh prepare | seq | simd | cuda | fpga | images"
        ;;
esac
