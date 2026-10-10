#!/usr/bin/env bash
# run_all.sh - corre cada motor UNA vez con los mismos parametros, guarda las
# imagenes en results/ y las compara contra sequential (deberian ser
# identicas: el reparto de trabajo no deberia cambiar el resultado, solo el
# tiempo). Es un chequeo rapido de que todo compila y corre bien, no mide
# rendimiento - para eso esta scripts/bench.py.
set -e
cd "$(dirname "$0")/.."

WIDTH=800; HEIGHT=600; SPHERES=12; DEPTH=4; THREADS=4
# cmp fija el hilo t a la CPU t: no se piden mas hilos que CPUs logicas (con HT apagado hay menos)
NCPU=$(nproc 2>/dev/null || echo "$THREADS")
if [ "$THREADS" -gt "$NCPU" ]; then THREADS=$NCPU; fi

# Hyper-Threading activo o apagado (scripts/ht_mode.py): cada modo guarda en su carpeta.
HT_MODE=$(python3 scripts/ht_mode.py 2>/dev/null || echo on)
IMG=results/ht_$HT_MODE/imagenes_raytracing
mkdir -p "$IMG"
echo "Hyper-Threading: $HT_MODE -> imagenes en $IMG"

echo "== sequential =="
./build/sequential $WIDTH $HEIGHT $SPHERES $DEPTH $IMG/run_sequential.bmp

echo "== coarseGrained (threads=$THREADS) =="
./build/coarseGrained $WIDTH $HEIGHT $SPHERES $DEPTH $THREADS $IMG/run_coarseGrained.bmp

echo "== fineGrained (threads=$THREADS) =="
./build/fineGrained $WIDTH $HEIGHT $SPHERES $DEPTH $THREADS $IMG/run_fineGrained.bmp

echo "== cmp (threads=$THREADS) =="
./build/cmp $WIDTH $HEIGHT $SPHERES $DEPTH $THREADS $IMG/run_cmp.bmp

if [ -x ./build/smt ]; then
    echo "== smt =="
    # 2 CPUs logicas del mismo nucleo fisico, con el mismo criterio que bench.py y perf_capture.sh
    CORE_IDS=$(bash scripts/detect_cpus.sh | sed -n 's/^sibling=//p')
    if [ -n "$CORE_IDS" ]; then
        echo "   (CPUs logicas hermanas detectadas automaticamente: $CORE_IDS)"
        ./build/smt $WIDTH $HEIGHT $SPHERES $DEPTH "$CORE_IDS" $IMG/run_smt.bmp
    else
        echo "   no hay 2 CPUs logicas hermanas (Hyper-Threading apagado?): se omite smt"
    fi
else
    echo "== smt == (no compilado en esta maquina - normal en macOS, se omite)"
fi

echo
echo "Comparando cada imagen contra sequential (deberian ser identicas byte a byte):"
for f in $IMG/run_coarseGrained.bmp $IMG/run_fineGrained.bmp $IMG/run_cmp.bmp $IMG/run_smt.bmp; do
    [ -f "$f" ] || continue
    if cmp -s $IMG/run_sequential.bmp "$f"; then
        echo "  OK   $f"
    else
        echo "  DIFF $f  <-- revisar, no deberia pasar"
    fi
done
