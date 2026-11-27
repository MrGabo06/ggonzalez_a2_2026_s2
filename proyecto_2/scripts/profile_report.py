#!/usr/bin/env python3
"""Resumen del perfilado de F4 a partir de lo que deja scripts/run_profile.sh.

Uso (desde proyecto_2):
    python3 scripts/profile_report.py <maquina>      # por defecto el hostname

Lee results/perfilado/<maquina>_perf.csv (perf) y results/perfilado/<maquina>_ncu/*.txt
(Nsight Compute) y escribe results/perfilado/<maquina>_perfilado.md con tablas.
Si falta una de las dos fuentes, resume solo la otra.
"""
import csv
import re
import socket
import sys
from pathlib import Path

OUT = Path("results/perfilado")

# Backend -> orden de aparicion en las tablas
ORDER = ["seq", "seq_sep", "simd", "simd_sep", "simd512", "simd_sep512", "cuda", "cuda_tiled"]


def rank(name):
    return ORDER.index(name) if name in ORDER else len(ORDER)


# ---------------------------------------------------------------- perf
def fnum(text):
    try:
        return float(text)
    except (TypeError, ValueError):
        return None


def perf_section(path):
    rows = list(csv.DictReader(open(path, newline="")))
    if not rows:
        return []
    lines = ["## CPU (perf)", ""]
    lines += [
        "Contadores de todo el proceso (incluye generar la imagen de entrada, igual para todos los",
        "backends), con `--reps/--warmup` iguales dentro de cada tamano. IPC = instrucciones / ciclos.",
        "Fallos de cache = fallos en la cache de ultimo nivel (LLC); L1 = fallos de carga en L1 de datos.",
        "",
    ]
    groups = {}
    for r in rows:
        groups.setdefault((r["filter"], int(r["size"])), []).append(r)
    for (flt, size) in sorted(groups):
        lines += [f"### {flt}, {size}x{size}", ""]
        lines += ["| backend | mediana (ms) | instrucciones (M) | ciclos (M) | IPC | fallos LLC (M) | fallos L1 (M) |",
                  "|---|---|---|---|---|---|---|"]
        for r in sorted(groups[(flt, size)], key=lambda r: rank(r["backend"])):
            ins, cyc = fnum(r["instructions"]), fnum(r["cycles"])
            ipc = f"{ins / cyc:.2f}" if ins and cyc else "-"

            def m(key):
                v = fnum(r[key])
                return f"{v / 1e6:.1f}" if v is not None else "-"

            med = fnum(r["median_ms"])
            lines.append(f"| {r['backend']} | {med:.3f} | {m('instructions')} | {m('cycles')} | {ipc} | "
                         f"{m('cache_misses')} | {m('l1d_load_misses')} |" if med is not None else
                         f"| {r['backend']} | - | {m('instructions')} | {m('cycles')} | {ipc} | "
                         f"{m('cache_misses')} | {m('l1d_load_misses')} |")
        lines.append("")
    return lines


# ----------------------------------------------------------------- ncu
# nombre en el reporte, unidad que lo identifica, tipo de numero
NCU_FIELDS = [
    ("duration_ms", r"Duration\s+(nsecond|usecond|msecond|second)\s+([\d.,]+)", "time"),
    ("cycles", r"Elapsed Cycles\s+cycle\s+([\d.,]+)", "int"),
    ("sm_pct", r"Compute \(SM\) Throughput\s+%\s+([\d.,]+)", "dec"),
    ("l1_pct", r"L1/TEX Cache Throughput\s+%\s+([\d.,]+)", "dec"),
    ("dram_pct", r"DRAM Throughput\s+%\s+([\d.,]+)", "dec"),
    ("l1_hit", r"L1/TEX Hit Rate\s+%\s+([\d.,]+)", "dec"),
    ("occupancy", r"Achieved Occupancy\s+%\s+([\d.,]+)", "dec"),
    ("warp_cpi", r"Warp Cycles Per Issued Instruction\s+cycle\s+([\d.,]+)", "dec"),
    ("ipc", r"Executed Ipc Active\s+inst/cycle\s+([\d.,]+)", "dec"),
    ("instr_m", r"Issued Instructions\s+inst\s+([\d.,]+)", "int"),
]


def to_float(token, kind):
    """Convierte '1.181.115', '85,992,306' o '80,58' sin depender del idioma del sistema."""
    if kind == "int":
        return float(re.sub(r"[.,]", "", token))
    # decimal: el ultimo separador es el decimal, los demas son de miles
    pos = max(token.rfind("."), token.rfind(","))
    if pos == -1:
        return float(token)
    return float(re.sub(r"[.,]", "", token[:pos]) + "." + token[pos + 1:])


def parse_ncu(path):
    text = path.read_text(errors="replace")
    vals = {}
    for key, pattern, kind in NCU_FIELDS:
        m = re.search(pattern, text)
        if not m:
            vals[key] = None
        elif kind == "time":
            scale = {"nsecond": 1e-6, "usecond": 1e-3, "msecond": 1.0, "second": 1e3}[m.group(1)]
            vals[key] = to_float(m.group(2), "dec") * scale
        else:
            vals[key] = to_float(m.group(1), kind)
    if vals["instr_m"] is not None:
        vals["instr_m"] /= 1e6
    return vals


def ncu_section(ncu_dir):
    files = sorted(ncu_dir.glob("*.txt"))
    if not files:
        return []
    data = {}
    for f in files:
        m = re.match(r"(.+)_(gauss|sobel)_(\d+)\.txt$", f.name)
        if m:
            data[(m.group(2), int(m.group(3)), m.group(1))] = parse_ncu(f)
    if not data:
        return []
    lines = ["## GPU (Nsight Compute)", ""]
    lines += [
        "Un kernel por backend. Nsight Compute fija el reloj de la GPU, asi que `duracion` sirve para",
        "comparar kernels entre si, no como tiempo absoluto (el CSV de `make bench` es el tiempo real).",
        "SM = throughput de computo, L1 = throughput de L1/TEX, DRAM = ancho de banda a memoria, todos en %",
        "del pico. IPC = instrucciones ejecutadas por ciclo activo.",
        "",
    ]

    def f(v, d=2):
        return "-" if v is None else f"{v:.{d}f}"

    for (flt, size) in sorted({(k[0], k[1]) for k in data}):
        lines += [f"### {flt}, {size}x{size}", ""]
        lines += ["| backend | duracion (ms) | ciclos (M) | instr. (M) | IPC | SM % | L1 % | DRAM % | L1 hit % | ocupacion % | ciclos/instr |",
                  "|---|---|---|---|---|---|---|---|---|---|---|"]
        names = sorted({k[2] for k in data if k[:2] == (flt, size)}, key=rank)
        for b in names:
            v = data[(flt, size, b)]
            cyc = None if v["cycles"] is None else v["cycles"] / 1e6
            lines.append(f"| {b} | {f(v['duration_ms'], 3)} | {f(cyc)} | {f(v['instr_m'], 1)} | {f(v['ipc'])} | "
                         f"{f(v['sm_pct'], 1)} | {f(v['l1_pct'], 1)} | {f(v['dram_pct'], 1)} | "
                         f"{f(v['l1_hit'], 1)} | {f(v['occupancy'], 1)} | {f(v['warp_cpi'])} |")
        a, b = data.get((flt, size, "cuda")), data.get((flt, size, "cuda_tiled"))
        if a and b and a["duration_ms"] and b["duration_ms"] and a["instr_m"] and b["instr_m"]:
            lines += ["",
                      f"cuda_tiled / cuda: duracion {b['duration_ms'] / a['duration_ms']:.2f}x, "
                      f"instrucciones {b['instr_m'] / a['instr_m']:.2f}x."]
        lines.append("")
    return lines


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else socket.gethostname()
    perf_csv = OUT / f"{host}_perf.csv"
    ncu_dir = OUT / f"{host}_ncu"
    lines = [f"# Perfilado: {host}", ""]
    body = []
    if perf_csv.exists():
        body += perf_section(perf_csv)
    if ncu_dir.is_dir():
        body += ncu_section(ncu_dir)
    if not body:
        print("Nada que resumir: no hay datos de perf ni de ncu para", host)
        return
    out = OUT / f"{host}_perfilado.md"
    out.write_text("\n".join(lines + body) + "\n")
    print(f"Resumen en {out}")


if __name__ == "__main__":
    main()
