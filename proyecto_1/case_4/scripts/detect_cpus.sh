#!/usr/bin/env bash
# detect_cpus.sh - propone IDs de CPU logica para el motor `smt`, leyendo la
# topologia con `lscpu`. Lo usan bench.py y perf_capture.sh.
# Salida (stdout), una linea por hallazgo; si no se puede determinar, no sale:
#   sibling=3,7    2 CPUs logicas del MISMO nucleo fisico (requiere SMT activo)
#   separate=0,1   2 CPUs logicas de nucleos fisicos DISTINTOS (control: los
#                  mismos 2 hilos, pero sin compartir nucleo)
lscpu -p=CPU,CORE 2>/dev/null | grep -v '^#' | awk -F, '
{
    cpu = $1 + 0; core = $2 + 0
    if (!(core in cnt)) first[core] = cpu      # lscpu lista las CPUs en orden
    cnt[core]++
    if (cnt[core] == 2) second[core] = cpu
}
END {
    sib = -1
    for (c in cnt) if (cnt[c] >= 2 && (sib < 0 || c + 0 < sib)) sib = c + 0
    if (sib >= 0) printf "sibling=%d,%d\n", first[sib], second[sib]

    a = -1; b = -1                              # los 2 nucleos de menor id, sin el hermano
    for (c in cnt) {
        if (sib >= 0 && c + 0 == sib) continue
        v = c + 0
        if (a < 0 || v < a) { b = a; a = v }
        else if (b < 0 || v < b) b = v
    }
    if (a >= 0 && b >= 0) printf "separate=%d,%d\n", first[a], first[b]
}'
