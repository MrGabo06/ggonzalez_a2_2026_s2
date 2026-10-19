#!/usr/bin/env python3
"""
merge_smt_rerun.py

Compares a partial re-measurement of the SMT model (made with
`WORKERS="8 16" scripts/campaign_smt.sh`, which writes
results/raw/final_smt_<on|off>_rerun.csv) against the original full-campaign
file and against the CMP file of the same session, and optionally replaces the
re-measured thread counts in the full file.

Why: the SMT model runs the same code as CMP, so in the same session both
should give the same times. This script shows, per (N, workers), the mean
ns/interaction of the old SMT rows, the new SMT rows and the CMP rows, plus the
SMT/CMP ratio before and after, so the decision to replace is made with the
numbers in view.

Usage (from the case_2 root):
    python3 scripts/merge_smt_rerun.py [on|off]            # report only (dry run)
    python3 scripts/merge_smt_rerun.py [on|off] --apply    # also write the merge

With --apply the original file is first copied to final_smt_<s>.csv.bak and
then rewritten: rows of the re-measured thread counts are replaced by the
rerun rows; all other rows are kept untouched.
"""

import os
import shutil
import sys

import pandas as pd

RAW_DIR = "results/raw"


def MeanTable(df):
    """Mean ns/interaction per (n, workers), ignoring the sequential rows."""
    return df[df["model"] != "sequential"].groupby(["n", "workers"])["ns_per_interaction"].mean()


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    apply = "--apply" in sys.argv[1:]
    session = args[0] if args else "on"
    if session not in ("on", "off"):
        print("La sesion debe ser 'on' u 'off'.")
        sys.exit(1)

    oldPath = os.path.join(RAW_DIR, f"final_smt_{session}.csv")
    newPath = os.path.join(RAW_DIR, f"final_smt_{session}_rerun.csv")
    cmpPath = os.path.join(RAW_DIR, f"final_cmp_{session}.csv")
    for path in (oldPath, newPath, cmpPath):
        if not os.path.exists(path):
            print(f"Falta {path}.")
            sys.exit(1)

    old = pd.read_csv(oldPath)
    new = pd.read_csv(newPath)
    cmp = pd.read_csv(cmpPath)
    if list(old.columns) != list(new.columns):
        print("Las columnas del rerun no coinciden con las del archivo original.")
        sys.exit(1)

    rerunWorkers = sorted(new["workers"].unique())
    counts = new.groupby(["n", "workers"]).size()
    print(f"Sesion SMT {session}. Hilos re-medidos: {rerunWorkers}")
    print(f"Repeticiones por configuracion en el rerun: min {counts.min()}, max {counts.max()}")

    # Same physics: the checksum of every N must match the original rows.
    for n in sorted(new["n"].unique()):
        a = set(old[old["n"] == n]["checksum"].round(4))
        b = set(new[new["n"] == n]["checksum"].round(4))
        if a != b:
            print(f"ERROR: checksum distinto para N={n}: original {sorted(a)} vs rerun {sorted(b)}")
            sys.exit(1)
    print("Checksums del rerun iguales a los del original para todos los N.\n")

    oldMean, newMean, cmpMean = MeanTable(old), MeanTable(new), MeanTable(cmp)
    rows = []
    for (n, w), newValue in newMean.items():
        oldValue = oldMean.get((n, w), float("nan"))
        cmpValue = cmpMean.get((n, w), float("nan"))
        rows.append((n, w, oldValue, newValue, cmpValue, oldValue / cmpValue, newValue / cmpValue))
    report = pd.DataFrame(rows, columns=["n", "workers", "smt_viejo", "smt_nuevo", "cmp",
                                         "viejo/cmp", "nuevo/cmp"])
    print("ns por interaccion (media). 'viejo/cmp' y 'nuevo/cmp' cerca de 1.00 = SMT igual a CMP\n")
    print(report.to_string(index=False, float_format=lambda x: f"{x:.3f}"))

    if not apply:
        print("\nSolo reporte. Para reemplazar esas filas en el archivo completo: "
              "agrega --apply (o `make merge-smt-rerun APPLY=1`).")
        return

    backup = oldPath + ".bak"
    shutil.copyfile(oldPath, backup)
    kept = old[~old["workers"].isin(rerunWorkers)]
    merged = pd.concat([kept, new], ignore_index=True)
    merged.to_csv(oldPath, index=False)
    print(f"\nRespaldo del original: {backup} (no lo agregues al commit).")
    print(f"Escrito {oldPath}: {len(kept)} filas conservadas + {len(new)} del rerun = {len(merged)}.")


if __name__ == "__main__":
    main()
