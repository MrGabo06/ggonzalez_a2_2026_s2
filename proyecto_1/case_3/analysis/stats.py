"""Summarises the bench CSVs: per-configuration mean, 95% CI, speedup and
efficiency, convergence of the CI with the number of runs, and perf counters.

Usage: python analysis/stats.py [data/bench_*.csv ...]
Writes results/summary.csv, results/convergence.csv, results/perf_summary.csv,
results/perf_per_cpu.csv and prints a Markdown report.
"""

import argparse
import math
import pathlib
import sys

import numpy as np
import pandas as pd
from scipy import stats

ROOT = pathlib.Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
RESULTS = ROOT / "results"

CONFIG = ["smt", "model", "N", "threads", "quantum"]
MODEL_ORDER = ["baseline_seq", "cmp", "smt", "fine", "coarse"]
PHYSICAL_CORES = 4
TARGET_REL_HALF_WIDTH = 0.01
CHECKPOINTS = [10, 25, 50, 100, 150, 200, 300, 400]
BOOTSTRAP = 2000


def load_runs(paths=None):
    paths = [pathlib.Path(p) for p in paths] if paths else sorted(DATA.glob("bench_*.csv"))
    if not paths:
        sys.exit(f"no bench CSVs in {DATA}: run 'make bench' first")
    frames = [pd.read_csv(p, dtype={"quantum": str}, keep_default_na=False) for p in paths]
    runs = pd.concat(frames, ignore_index=True)
    runs["model"] = pd.Categorical(runs["model"], MODEL_ORDER, ordered=True)
    return runs


def cores_used(model, threads, smt):
    if model in ("baseline_seq", "fine", "coarse"):
        return 1
    if model == "smt" and smt == "on":
        return math.ceil(threads / 2)
    return min(threads, PHYSICAL_CORES)


def t_half_width(values):
    n = len(values)
    if n < 2:
        return float("nan")
    return stats.t.ppf(0.975, n - 1) * np.std(values, ddof=1) / math.sqrt(n)


def bootstrap_speedup_ci(base, cand, rng):
    base_means = rng.choice(base, (BOOTSTRAP, len(base))).mean(axis=1)
    cand_means = rng.choice(cand, (BOOTSTRAP, len(cand))).mean(axis=1)
    return np.percentile(base_means / cand_means, [2.5, 97.5])


def summarise(runs):
    rng = np.random.default_rng(0)
    rows = []
    for key, group in runs.groupby(CONFIG, observed=True, sort=True):
        times = group["time_s"].to_numpy()
        mean = times.mean()
        std = times.std(ddof=1) if len(times) > 1 else float("nan")
        half = t_half_width(times)
        t_crit = stats.t.ppf(0.975, max(len(times) - 1, 1))
        rows.append({
            **dict(zip(CONFIG, key)),
            "steps": group["steps"].iloc[0],
            "n": len(times),
            "mean_s": mean,
            "std_s": std,
            "median_s": np.median(times),
            "q1_s": np.percentile(times, 25),
            "q3_s": np.percentile(times, 75),
            "ci_low_s": mean - half,
            "ci_high_s": mean + half,
            "ci_half_pct": 100 * half / mean,
            "cv_pct": 100 * std / mean,
            "n_for_1pct": math.ceil((t_crit * std / (TARGET_REL_HALF_WIDTH * mean)) ** 2),
            "mean_freq_mhz": group["freq_mhz"].mean(),
            "max_temp_c": group["temp_c"].max(),
            "cores": cores_used(key[1], key[3], key[0]),
        })
    summary = pd.DataFrame(rows)

    for column in ("speedup", "speedup_ci_low", "speedup_ci_high", "efficiency"):
        summary[column] = np.nan
    for (smt, n), block in summary.groupby(["smt", "N"]):
        base_mask = (runs["smt"] == smt) & (runs["N"] == n) & (runs["model"] == "baseline_seq")
        base = runs.loc[base_mask, "time_s"].to_numpy()
        if not len(base):
            continue
        for idx, row in block.iterrows():
            cand_mask = (runs["smt"] == smt) & (runs["N"] == n) & (runs["model"] == row["model"]) & \
                        (runs["threads"] == row["threads"]) & (runs["quantum"] == row["quantum"])
            cand = runs.loc[cand_mask, "time_s"].to_numpy()
            speedup = base.mean() / row["mean_s"]
            low, high = bootstrap_speedup_ci(base, cand, rng)
            summary.loc[idx, ["speedup", "speedup_ci_low", "speedup_ci_high", "efficiency"]] = \
                [speedup, low, high, speedup / row["cores"]]
    return summary


def convergence(runs):
    rows = []
    for key, group in runs.groupby(CONFIG, observed=True, sort=True):
        times = group.sort_values(["timestamp"], kind="stable")["time_s"].to_numpy()
        final_mean = times.mean()
        for n in [c for c in CHECKPOINTS if c <= len(times)] + [len(times)]:
            prefix = times[:n]
            rows.append({
                **dict(zip(CONFIG, key)),
                "n": n,
                "mean_s": prefix.mean(),
                "ci_half_pct": 100 * t_half_width(prefix) / prefix.mean(),
                "drift_from_final_pct": 100 * abs(prefix.mean() - final_mean) / final_mean,
            })
    return pd.DataFrame(rows).drop_duplicates(CONFIG + ["n"])


def checksum_mismatches(runs):
    per_problem = runs.groupby(["N", "steps"])["checksum"].nunique()
    return per_problem[per_problem > 1]


def parse_perf_file(path):
    rows = []
    for line in path.read_text().splitlines():
        fields = line.split(",")
        if not line or line.startswith("#") or len(fields) < 4 or not fields[0].startswith("CPU"):
            continue
        value = fields[1]
        if not value.replace(".", "", 1).isdigit():
            continue
        rows.append({"cpu": int(fields[0][3:]), "event": fields[3], "value": float(value)})
    return pd.DataFrame(rows)


def perf_tables():
    files = sorted((DATA / "perf").glob("smt*/*.csv"))
    totals, per_cpu = [], []
    for path in files:
        counts = parse_perf_file(path)
        if counts.empty:
            continue
        model, n, t = path.stem.rsplit("_", 2)
        meta = {"smt": path.parent.name.removeprefix("smt"), "model": model.removeprefix("model_"),
                "N": int(n[1:]), "threads": int(t[1:])}
        event = counts.groupby("event")["value"].sum()
        get = lambda name: event.get(name, np.nan)
        cycles = get("cycles")
        totals.append({
            **meta,
            "ipc": get("instructions") / cycles,
            "cache_miss_pct": 100 * get("cache-misses") / get("cache-references"),
            "llc_mpki": 1000 * get("LLC-load-misses") / get("instructions"),
            "stalls_total_pct": 100 * get("cycle_activity.stalls_total") / cycles,
            "stalls_l2_miss_pct": 100 * get("cycle_activity.stalls_l2_miss") / cycles,
            "stalls_l3_miss_pct": 100 * get("cycle_activity.stalls_l3_miss") / cycles,
            "context_switches": get("context-switches"),
            "cpu_migrations": get("cpu-migrations"),
        })
        cpu_cycles = counts[counts["event"] == "cycles"].set_index("cpu")["value"]
        cpu_instr = counts[counts["event"] == "instructions"].set_index("cpu")["value"]
        for cpu, cyc in cpu_cycles.items():
            per_cpu.append({**meta, "cpu": cpu, "cycles": cyc,
                            "busy_share_pct": 100 * cyc / cpu_cycles.max(),
                            "ipc": cpu_instr.get(cpu, np.nan) / cyc if cyc else np.nan})
    return pd.DataFrame(totals), pd.DataFrame(per_cpu)


def markdown_report(summary, conv, dropped, mismatches):
    lines = ["# Benchmark summary", ""]
    lines.append(f"Runs dropped for thermal throttling: {dropped}")
    if len(mismatches):
        lines.append(f"**Checksum mismatches** in {len(mismatches)} (N, steps) problems: {mismatches.to_dict()}")
    else:
        lines.append("Checksums: identical across every model for each (N, steps).")
    lines += ["", "| smt | model | N | T | q | n | mean (s) | 95% CI (s) | ±% | speedup [95% CI] | eff. |",
              "|---|---|---|---|---|---|---|---|---|---|---|"]
    for _, r in summary.iterrows():
        lines.append(
            f"| {r.smt} | {r.model} | {r.N} | {r.threads} | {r.quantum} | {r.n} | {r.mean_s:.4f} | "
            f"[{r.ci_low_s:.4f}, {r.ci_high_s:.4f}] | {r.ci_half_pct:.2f} | "
            f"{r.speedup:.2f} [{r.speedup_ci_low:.2f}, {r.speedup_ci_high:.2f}] | {r.efficiency:.2f} |")

    final = conv.sort_values("n").groupby(CONFIG, observed=True).tail(1)
    within = (final["ci_half_pct"] <= 100 * TARGET_REL_HALF_WIDTH).sum()
    lines += ["", "## Convergence", "",
              f"{within}/{len(final)} configurations reach a 95% CI within ±1% of the mean.",
              f"Largest n needed for ±1%: {summary['n_for_1pct'].max()} "
              f"({summary.loc[summary['n_for_1pct'].idxmax(), CONFIG].to_dict()}).",
              f"Largest drift of the mean between n=100 and the final n: "
              f"{conv[conv['n'] == 100]['drift_from_final_pct'].max():.2f}%."]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", nargs="*", help="bench CSVs (default: data/bench_*.csv)")
    args = parser.parse_args()

    runs = load_runs(args.csv)
    dropped = int(runs["throttled"].sum())
    runs = runs[runs["throttled"] == 0]
    mismatches = checksum_mismatches(runs)

    RESULTS.mkdir(exist_ok=True)
    summary = summarise(runs)
    conv = convergence(runs)
    summary.to_csv(RESULTS / "summary.csv", index=False)
    conv.to_csv(RESULTS / "convergence.csv", index=False)

    perf_totals, perf_cpu = perf_tables()
    if not perf_totals.empty:
        perf_totals.to_csv(RESULTS / "perf_summary.csv", index=False)
        perf_cpu.to_csv(RESULTS / "perf_per_cpu.csv", index=False)

    report = markdown_report(summary, conv, dropped, mismatches)
    (RESULTS / "summary.md").write_text(report + "\n")
    print(report)
    return 1 if len(mismatches) else 0


if __name__ == "__main__":
    sys.exit(main())
