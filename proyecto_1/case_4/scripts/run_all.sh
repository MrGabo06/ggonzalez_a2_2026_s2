#!/usr/bin/env bash
# run_all.sh - corre cada motor UNA vez con los mismos parametros, guarda las
# imagenes en results/ y las compara contra sequential (deberian ser
# identicas: el reparto de trabajo no deberia cambiar el resultado, solo el
# tiempo). Es un chequeo rapido de que todo compila y corre bien, no mide
# rendimiento - para eso esta scripts/bench.py.
set -e
cd "$(dirname "$0")/.."

WIDTH=800; HEIGHT=600; SPHERES=12; DEPTH=4; THREADS=4
mkdir -p results

echo "== sequential =="
./build/sequential $WIDTH $HEIGHT $SPHERES $DEPTH results/run_sequential.bmp

echo "== coarseGrained (threads=$THREADS) =="
./build/coarseGrained $WIDTH $HEIGHT $SPHERES $DEPTH $THREADS results/run_coarseGrained.bmp

echo "== fineGrained (threads=$THREADS) =="
./build/fineGrained $WIDTH $HEIGHT $SPHERES $DEPTH $THREADS results/run_fineGrained.bmp

echo "== cmp (threads=$THREADS) =="
./build/cmp $WIDTH $HEIGHT $SPHERES $DEPTH $THREADS results/run_cmp.bmp

if [ -x ./build/smt ]; then
    echo "== smt =="
    # detecta 2 CPUs logicas que comparten el mismo nucleo fisico (columna CORE de lscpu -p)
    CORE_IDS=$(lscpu -p=CPU,CORE 2>/dev/null | grep -v '^#' | awk -F, '
        { core[$2] = core[$2] "," $1 }
        END {
            for (c in core) {
                n = split(substr(core[c], 2), cpus, ",")
                if (n >= 2) { print cpus[1] "," cpus[2]; exit }
            }
        }')
    if [ -n "$CORE_IDS" ]; then
        echo "   (CPUs logicas hermanas detectadas automaticamente: $CORE_IDS)"
        ./build/smt $WIDTH $HEIGHT $SPHERES $DEPTH "$CORE_IDS" results/run_smt.bmp
    else
        echo "   no pude detectar 2 CPUs logicas hermanas automaticamente (revisa 'lscpu -e' a mano y corre smt vos misma)"
    fi
else
    echo "== smt == (no compilado en esta maquina - normal en macOS, se omite)"
fi

echo
echo "Comparando cada imagen contra sequential (deberian ser identicas byte a byte):"
for f in results/run_coarseGrained.bmp results/run_fineGrained.bmp results/run_cmp.bmp results/run_smt.bmp; do
    [ -f "$f" ] || continue
    if cmp -s results/run_sequential.bmp "$f"; then
        echo "  OK   $f"
    else
        echo "  DIFF $f  <-- revisar, no deberia pasar"
    fi
done
