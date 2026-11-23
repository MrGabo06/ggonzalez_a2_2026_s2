#!/usr/bin/env python3
"""Numeric differences between the CPU, GPU and FPGA outputs.

Usage (from proyecto_2):
    python3 scripts/numeric_diff.py export    # run the available backends here, write the digests
    python3 scripts/numeric_diff.py report    # merge every machine's digests into a table

export runs dlp_bench with --hex-dir (same image: filter, size and seed) for every backend that is
available on this machine and stores the SHA-256 of the input and of each output in
results/numeric/<host>_digest.csv. The .hex files stay in build/numeric/. Each machine only needs to
run export once; the small digest CSVs are what gets committed.

report compares digests, so it only needs the CSVs of all machines. Equal digests mean the outputs
are identical byte by byte: maximum difference 0 and 0 different pixels. Outputs only count as
comparable when the input digests are equal too. A second section derives the same pairs from
the validation against seq (max_abs_error) of the benchmark CSVs.
"""
import argparse
import csv
import hashlib
import socket
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "build" / "dlp_bench"
DIGEST_DIR = ROOT / "results" / "numeric"
ALL_BACKENDS = ["seq", "seq_sep", "simd", "simd_sep", "simd512", "simd_sep512", "cuda", "cuda_tiled", "fpga"]
CLASSES = {
    "CPU": {"seq", "seq_sep", "simd", "simd_sep", "simd512", "simd_sep512"},
    "GPU": {"cuda", "cuda_tiled"},
    "FPGA": {"fpga"},
}
CASES = [("CPU + GPU", ["CPU", "GPU"]), ("CPU + FPGA", ["CPU", "FPGA"]),
         ("GPU + CPU + FPGA", ["GPU", "CPU", "FPGA"])]
FIELDS = ["host", "backend", "filter", "size", "seed", "sha256_in", "sha256_out"]

WHY_ZERO = """## Por qué la diferencia es 0

CPU y GPU calculan en `float32` y la FPGA en enteros, así que el resultado idéntico no es obvio. Se
cumple porque todos los pesos son múltiplos de 1/256 (Gauss: `k/256` con k entero; Sobel: enteros) y
las entradas son bytes. Cada producto y cada suma parcial es un múltiplo de 1/256 con valor absoluto
menor que 1024 (Sobel llega a ±4·255 = ±1020 antes del valor absoluto), así que necesita como máximo
18 bits significativos (10 enteros y 8 fraccionarios), menos que los 24 de la mantisa de `float32`:
se representa sin redondeo. Por eso el orden de la suma (secuencial, SIMD, hilos de la GPU) y el uso
de FMA no cambian el resultado. El redondeo
final del baseline, `trunc(v + 0.5)`, coincide con el `(acc + 128) >> 8` de la FPGA para valores
no negativos, y Sobel aplica valor absoluto y saturación igual en todos.
"""


def sha256_of(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def export(args):
    if not BIN.exists():
        sys.exit(f"No existe {BIN}. Compile primero con make build (SIMD=1 AVX512=1 CUDA=1)")
    host = args.host or socket.gethostname()
    hex_dir = ROOT / "build" / "numeric" / host
    hex_dir.mkdir(parents=True, exist_ok=True)
    rows, skipped = [], set()
    for backend in args.backends:
        for flt in args.filters:
            for size in args.sizes:
                if backend in skipped:
                    continue
                cmd = [str(BIN), "--backend", backend, "--filter", flt, "--size", str(size),
                       "--seed", str(args.seed), "--reps", "1", "--warmup", "0",
                       "--hex-dir", str(hex_dir), "--output", "/dev/null"]
                done = subprocess.run(cmd, capture_output=True, text=True)
                base = f"{flt}_{size}x{size}_seed{args.seed}"
                out_hex, in_hex = hex_dir / f"{base}_{backend}_out.hex", hex_dir / f"{base}_in.hex"
                if done.returncode != 0 or not out_hex.exists():
                    reason = (done.stderr.strip().splitlines() or ["sin salida"])[0]
                    print(f"omitido  {backend}: {reason}")
                    skipped.add(backend)
                    continue
                rows.append({"host": host, "backend": backend, "filter": flt, "size": size,
                             "seed": args.seed, "sha256_in": sha256_of(in_hex),
                             "sha256_out": sha256_of(out_hex)})
                print(f"ok       {backend} {flt} {size}x{size}")
    if not rows:
        sys.exit("Ningun backend produjo salida")
    DIGEST_DIR.mkdir(parents=True, exist_ok=True)
    target = DIGEST_DIR / f"{host}_digest.csv"
    with open(target, "w", newline="") as f:
        writer = csv.DictWriter(f, FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    print(f"{len(rows)} digests en {target}")


def klass(backend):
    return next((name for name, members in CLASSES.items() if backend in members), "otro")


def case_status(rows, needed):
    present = {klass(r["backend"]) for r in rows}
    missing = [c for c in needed if c not in present]
    if missing:
        return f"pendiente: falta {', '.join(missing)} (correr `make numeric_export` en esa máquina)"
    used = [r for r in rows if klass(r["backend"]) in needed]
    if len({r["sha256_in"] for r in used}) > 1:
        return "no comparable: la imagen de entrada no es idéntica entre máquinas"
    outputs = {}
    for r in used:
        outputs.setdefault(r["sha256_out"], []).append(f"{r['backend']}@{r['host']}")
    if len(outputs) == 1:
        return f"**idénticos bit a bit** (diferencia máxima 0, 0 píxeles distintos; {len(used)} salidas)"
    groups = " / ".join(", ".join(g) for g in outputs.values())
    return f"**DIFIEREN**: grupos {groups}"


def digest_sections(rows):
    keys = sorted({(r["filter"], int(r["size"]), r["seed"]) for r in rows})
    lines = ["## Comparación directa (digest SHA-256 de las salidas)", "",
             "| Caso | Filtro | Tamaño | Resultado |", "|---|---|---|---|"]
    for flt, size, seed in keys:
        group = [r for r in rows if (r["filter"], int(r["size"]), r["seed"]) == (flt, size, seed)]
        for label, needed in CASES:
            lines.append(f"| {label} | {flt} | {size}² (semilla {seed}) | {case_status(group, needed)} |")
    lines += ["", "### Inventario de digests", "", "| Máquina | Backend | Filtro | Tamaño | entrada | salida |",
              "|---|---|---|---|---|---|"]
    for r in sorted(rows, key=lambda r: (r["filter"], int(r["size"]), r["host"], r["backend"])):
        lines.append(f"| {r['host']} | {r['backend']} | {r['filter']} | {r['size']}² | "
                     f"`{r['sha256_in'][:10]}` | `{r['sha256_out'][:10]}` |")
    return lines + [""]


def validation_errors():
    """{(class, filter, size): max error against seq} from every benchmark CSV."""
    errors = {}
    paths = list((ROOT / "results").glob("*.csv")) + list((ROOT / "results" / "fpga").glob("*.csv"))
    for path in paths:
        with open(path, newline="") as f:
            for r in csv.DictReader(f):
                if r["max_abs_error"] == "" or klass(r["backend"]) == "otro":
                    continue
                key = (klass(r["backend"]), r["filter"], int(r["width"]))
                errors[key] = max(errors.get(key, 0), int(r["max_abs_error"]))
    return errors


def derived_section(hosts):
    errors = validation_errors()
    sizes = sorted({k[2] for k in errors})
    lines = ["## Derivado de la validación contra seq", "",
             "Cada backend se validó contra `seq` con el mismo filtro, tamaño y semilla. Como "
             "|A − B| ≤ |A − seq| + |B − seq|, el error entre dos plataformas está acotado por la suma "
             "de sus errores contra `seq`. Solo vale si la imagen y la salida de `seq` son iguales en "
             "todas las máquinas. Eso lo confirman los digests, y hoy hay digests de: "
             f"{', '.join(hosts)}. Las filas que mezclan máquinas sin digest propio son derivadas, "
             "no verificadas.", "",
             "| Caso | Filtro | Tamaño | Error de cada clase | Cota de la diferencia |", "|---|---|---|---|---|"]
    for label, needed in CASES:
        for flt in ("gauss", "sobel"):
            for size in sizes:
                parts = [errors.get((c, flt, size)) for c in needed]
                if None in parts:
                    continue
                detail = ", ".join(f"{c} {e}" for c, e in zip(needed, parts))
                lines.append(f"| {label} | {flt} | {size}² | {detail} | {sum(parts)} |")
    return lines + [""]


def report(_args):
    rows = []
    for path in sorted(DIGEST_DIR.glob("*_digest.csv")):
        with open(path, newline="") as f:
            rows += list(csv.DictReader(f))
    if not rows:
        sys.exit("No hay digests en results/numeric/. Corra primero make numeric_export")
    lines = ["# Diferencias numéricas entre CPU, GPU y FPGA", ""]
    lines += digest_sections(rows) + derived_section(sorted({r['host'] for r in rows})) + [WHY_ZERO]
    out = DIGEST_DIR / "diferencias.md"
    out.write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(lines))
    print(f"Escrito en {out}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    exp = sub.add_parser("export")
    exp.add_argument("--sizes", type=int, nargs="+", default=[64, 512])
    exp.add_argument("--filters", nargs="+", default=["gauss", "sobel"])
    exp.add_argument("--backends", nargs="+", default=ALL_BACKENDS)
    exp.add_argument("--seed", type=int, default=42)
    exp.add_argument("--host", help="nombre de la maquina en el CSV (por defecto el hostname)")
    exp.set_defaults(run=export)
    rep = sub.add_parser("report")
    rep.set_defaults(run=report)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
