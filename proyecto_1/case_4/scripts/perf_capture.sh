#!/usr/bin/env bash
# perf_capture.sh - captura contadores de hardware con `perf stat` para cada
# motor del ray tracer (requisito del enunciado: ciclos, instrucciones, stalls
# y uso por hilo logico, relacionados con los 4 modelos).
#
# Cada motor se corre REPS veces seguidas bajo UN solo `perf stat` (una corrida
# dura ~50 ms, muy poco para contar bien). Con -a -A perf cuenta todo el sistema
# y separa los contadores POR CPU LOGICA: se ve cuales trabajaron y cuales no.
# El texto crudo queda en results/perf/<motor>.txt (sirve de captura para el
# documento); perf calcula solo "insn per cycle" (IPC) si hay cycles+instructions.
#
# Uso (Linux fisico, desde proyecto_1/case_4):
#     make perf                      # o: bash scripts/perf_capture.sh
#     THREADS=8 REPS=50 make perf    # parametros opcionales
# Conviene dejar la maquina sin otra carga mientras corre.
set -u
cd "$(dirname "$0")/.."

WIDTH=800; HEIGHT=600; SPHERES=12; DEPTH=4
THREADS=${THREADS:-4}
REPS=${REPS:-20}
OUTDIR=results/perf

if ! command -v perf >/dev/null 2>&1; then
    echo "perf no esta instalado. En Ubuntu: sudo apt install linux-tools-common linux-tools-\$(uname -r)"
    echo "(si no tenes sudo en esta maquina, pedile al administrador que lo instale)"
    exit 1
fi
mkdir -p "$OUTDIR"
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

# --- modo de conteo: por CPU logica (-a -A) si el sistema lo permite ---------
PERF_FLAGS=""
if perf stat -a -A -e cycles -- true >"$TMP" 2>&1 && ! grep -qE 'not supported|not counted|Permission|permiso' "$TMP"; then
    PERF_FLAGS="-a -A"
else
    echo "AVISO: perf no permite contar todo el sistema (-a) con tu usuario; cuento solo el proceso, SIN desglose por CPU logica."
    echo "       perf_event_paranoid = $(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo '?')  (para el desglose por CPU hace falta <= 0, o root/CAP_PERFMON)"
    # Si ni siquiera el conteo basico del propio proceso funciona, no tiene sentido seguir.
    if ! perf stat -e cycles -- true >"$TMP" 2>&1 || grep -qE 'not supported|not counted' "$TMP"; then
        echo
        echo "perf no puede leer contadores de hardware en esta maquina. Salida de perf:"
        head -n 8 "$TMP"
        echo
        echo "Causas tipicas: kernel.perf_event_paranoid muy alto (cambiarlo requiere administrador), o VM/contenedor sin contadores."
        echo "El enunciado exige medir en hardware fisico dedicado."
        exit 1
    fi
fi

# --- eventos: cycles+instructions siempre; el resto solo si esta CPU los tiene ----
# stalls y eventos de SMT cambian de nombre segun la microarquitectura, por eso
# cada candidato se prueba antes y se descarta si no existe/no esta soportado.
EVENTS="cycles,instructions"
CANDIDATES="stalled-cycles-frontend stalled-cycles-backend cycle_activity.stalls_total resource_stalls.any cpu_clk_unhalted.one_thread_active cache-misses"
for ev in $CANDIDATES; do
    if perf stat $PERF_FLAGS -e "$ev" -- true >"$TMP" 2>&1 && ! grep -qE 'not supported|not counted|unknown|invalid|syntax' "$TMP"; then
        EVENTS="$EVENTS,$ev"
    fi
done
echo "Eventos: $EVENTS"
echo "Modo:    ${PERF_FLAGS:-solo proceso}   |   REPS=$REPS   THREADS=$THREADS"
echo

capture() {   # capture <etiqueta> <comando...>
    local label=$1; shift
    local cmd="$*"
    local out="$OUTDIR/$label.txt"
    echo "== $label: $cmd  (x$REPS) =="
    {
        echo "# $label | $(date -Is) | $(uname -r) | modo: ${PERF_FLAGS:-solo proceso}"
        echo "# comando (repetido $REPS veces): $cmd"
        echo "# eventos: $EVENTS"
    } > "$out"
    perf stat $PERF_FLAGS -e "$EVENTS" -- bash -c "for i in \$(seq $REPS); do $cmd >/dev/null 2>&1; done" 2>> "$out"
    cat "$out"
    echo
}

COMMON="$WIDTH $HEIGHT $SPHERES $DEPTH"
capture sequential    ./build/sequential    $COMMON
capture fineGrained   ./build/fineGrained   $COMMON $THREADS
capture coarseGrained ./build/coarseGrained $COMMON $THREADS
capture cmp           ./build/cmp           $COMMON $THREADS

if [ -x ./build/smt ]; then
    DET=$(bash scripts/detect_cpus.sh)
    SIB=$(echo "$DET" | sed -n 's/^sibling=//p')
    SEP=$(echo "$DET" | sed -n 's/^separate=//p')
    if [ -n "$SIB" ]; then capture smt      ./build/smt $COMMON "$SIB"
    else echo "smt: no hay CPUs logicas hermanas (SMT apagado?) -> se omite"; fi
    if [ -n "$SEP" ]; then capture smt_ctrl ./build/smt $COMMON "$SEP"; fi
else
    echo "smt: build/smt no existe (solo compila en Linux) -> se omite"
fi

echo "Listo. Capturas en $OUTDIR/*.txt"
