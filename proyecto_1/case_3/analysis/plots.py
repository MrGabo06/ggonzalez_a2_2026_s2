"""Draws every figure the report needs from the bench CSVs and the tables that
stats.py leaves in results/. Each figure is saved as PNG and PDF.

Usage: python analysis/plots.py [data/bench_*.csv ...]
"""

import argparse

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

import stats

COLORS = {"baseline_seq": "#2a78d6", "cmp": "#eb6834", "smt": "#1baf7a",
          "fine": "#eda100", "coarse": "#e87ba4"}
MARKERS = {"baseline_seq": "o", "cmp": "s", "smt": "^", "fine": "D", "coarse": "v"}
SMT_STATE = {"on": "#2a78d6", "off": "#eb6834"}
INK, MUTED, GRID = "#0b0b0b", "#52514e", "#e4e3df"

plt.rcParams.update({
    "figure.dpi": 120, "savefig.dpi": 200, "font.size": 9,
    "axes.edgecolor": MUTED, "axes.labelcolor": INK, "axes.titlesize": 10,
    "axes.spines.top": False, "axes.spines.right": False,
    "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6, "axes.axisbelow": True,
    "xtick.color": MUTED, "ytick.color": MUTED, "legend.frameon": False,
    "lines.linewidth": 2, "lines.markersize": 6,
})


def label(row):
    text = row["model"] if row["model"] == "baseline_seq" else f"{row['model']} T={row['threads']}"
    return text + (f" q={row['quantum']}" if row["quantum"] != "NA" else "")


def save(fig, name):
    for ext in ("png", "pdf"):
        fig.savefig(stats.RESULTS / f"{name}.{ext}", bbox_inches="tight")
    plt.close(fig)
    print(f"wrote results/{name}.png/.pdf")


def ns_of(frame):
    return sorted(frame["N"].unique())


def boxplots(runs, smt):
    data = runs[runs["smt"] == smt]
    ns = ns_of(data)
    fig, axes = plt.subplots(len(ns), 1, figsize=(8, 2.6 * len(ns)), squeeze=False)
    for ax, n in zip(axes[:, 0], ns):
        groups = [(label(key_row), g["time_s"].to_numpy(), key_row["model"])
                  for _, g in data[data["N"] == n].groupby(stats.CONFIG[1:], observed=True)
                  for key_row in [g.iloc[0]]]
        box = ax.boxplot([g[1] for g in groups], tick_labels=[g[0] for g in groups], patch_artist=True,
                         widths=0.6, flierprops={"markersize": 3, "markeredgecolor": MUTED},
                         medianprops={"color": INK})
        for patch, (_, _, model) in zip(box["boxes"], groups):
            patch.set(facecolor=COLORS[model], alpha=0.75, edgecolor=MUTED)
        ax.set_title(f"N={n} (steps={data[data['N'] == n]['steps'].iloc[0]})", loc="left")
        ax.set_ylabel("time (s)")
        ax.tick_params(axis="x", rotation=30)
    fig.suptitle(f"Run-time distribution per configuration, SMT {smt}", x=0.01, ha="left")
    fig.tight_layout()
    save(fig, f"boxplot_smt{smt}")


def histograms(runs, smt):
    data = runs[runs["smt"] == smt]
    for n in ns_of(data):
        groups = list(data[data["N"] == n].groupby(stats.CONFIG[1:], observed=True))
        cols = 4
        rows = int(np.ceil(len(groups) / cols))
        fig, axes = plt.subplots(rows, cols, figsize=(10, 2.2 * rows), squeeze=False)
        for ax, (_, g) in zip(axes.flat, groups):
            model = g["model"].iloc[0]
            ax.hist(g["time_s"], bins=30, color=COLORS[model], edgecolor="white", linewidth=0.5)
            ax.axvline(g["time_s"].mean(), color=INK, linewidth=1)
            ax.set_title(label(g.iloc[0]), loc="left", fontsize=9)
            ax.set_xlabel("time (s)")
        for ax in axes.flat[len(groups):]:
            ax.set_visible(False)
        fig.suptitle(f"Run-time histograms, N={n}, SMT {smt} (line = mean)", x=0.01, ha="left")
        fig.tight_layout()
        save(fig, f"hist_N{n}_smt{smt}")


def scaling(summary, smt, metric, ylabel, name, reference=None):
    data = summary[summary["smt"] == smt]
    ns = ns_of(data)
    fig, axes = plt.subplots(1, len(ns), figsize=(3.2 * len(ns), 3.2), sharey=True, squeeze=False)
    for ax, n in zip(axes[0], ns):
        block = data[data["N"] == n]
        for model in stats.MODEL_ORDER:
            series = block[block["model"] == model].sort_values("threads")
            if series.empty:
                continue
            kwargs = {"color": COLORS[model], "marker": MARKERS[model], "label": model}
            if metric == "speedup":
                err = [series["speedup"] - series["speedup_ci_low"], series["speedup_ci_high"] - series["speedup"]]
                ax.errorbar(series["threads"], series[metric], yerr=err, capsize=2, **kwargs)
            else:
                ax.plot(series["threads"], series[metric], **kwargs)
        if reference is not None:
            ax.axhline(reference, color=MUTED, linewidth=1, linestyle="--")
        ax.set_title(f"N={n}", loc="left")
        ax.set_xlabel("threads")
        ax.set_xticks([1, 2, 4, 8])
    axes[0, 0].set_ylabel(ylabel)
    axes[0, -1].legend(loc="upper left", bbox_to_anchor=(1, 1))
    fig.suptitle(f"{name.capitalize()} by thread count, SMT {smt}", x=0.01, ha="left")
    fig.tight_layout()
    save(fig, f"{name}_smt{smt}")


def smt_on_off(summary):
    data = summary[summary["model"].isin(["cmp", "smt"])]
    if data["smt"].nunique() < 2:
        print("skip smt_on_off: needs bench CSVs with SMT on and off")
        return
    ns = ns_of(data)
    fig, axes = plt.subplots(1, len(ns), figsize=(3.2 * len(ns), 3.2), squeeze=False)
    for ax, n in zip(axes[0], ns):
        block = data[data["N"] == n]
        configs = block[["model", "threads"]].drop_duplicates().sort_values(["model", "threads"])
        x = np.arange(len(configs))
        for i, (state, offset) in enumerate((("on", -0.2), ("off", 0.2))):
            vals = configs.merge(block[block["smt"] == state], on=["model", "threads"], how="left")
            err = [vals["mean_s"] - vals["ci_low_s"], vals["ci_high_s"] - vals["mean_s"]]
            ax.bar(x + offset, vals["mean_s"], width=0.38, color=SMT_STATE[state], yerr=err,
                   capsize=2, edgecolor="white", linewidth=1, label=f"SMT {state}")
        ax.set_xticks(x, [f"{m} T={t}" for m, t in zip(configs["model"], configs["threads"])], rotation=40)
        ax.set_title(f"N={n}", loc="left")
    axes[0, 0].set_ylabel("mean time (s), 95% CI")
    axes[0, -1].legend(loc="upper left", bbox_to_anchor=(1, 1))
    fig.suptitle("Hyper-Threading on vs off (BIOS)", x=0.01, ha="left")
    fig.tight_layout()
    save(fig, "smt_on_off")


def convergence_plot(conv):
    ns = ns_of(conv)
    fig, axes = plt.subplots(1, len(ns), figsize=(3.2 * len(ns), 3.2), sharey=True, squeeze=False)
    for ax, n in zip(axes[0], ns):
        for key, g in conv[conv["N"] == n].groupby(stats.CONFIG, observed=True):
            ax.plot(g["n"], g["ci_half_pct"], color=COLORS[key[1]], marker=MARKERS[key[1]],
                    markersize=4, linewidth=1.2, alpha=0.8)
        ax.axhline(100 * stats.TARGET_REL_HALF_WIDTH, color=MUTED, linestyle="--", linewidth=1)
        ax.set_xscale("log")
        ax.set_title(f"N={n}", loc="left")
        ax.set_xlabel("runs (n)")
    axes[0, 0].set_ylabel("95% CI half-width (% of mean)")
    handles = [plt.Line2D([], [], color=COLORS[m], marker=MARKERS[m], label=m) for m in stats.MODEL_ORDER]
    axes[0, -1].legend(handles=handles, loc="upper left", bbox_to_anchor=(1, 1))
    fig.suptitle("Convergence of the 95% CI with the number of runs (dashed = ±1%)", x=0.01, ha="left")
    fig.tight_layout()
    save(fig, "convergence")


def perf_plots():
    path = stats.RESULTS / "perf_summary.csv"
    if not path.exists():
        print("skip perf plots: run 'make perf' and 'make stats' first")
        return
    perf = pd.read_csv(path)
    for smt, block in perf.groupby("smt"):
        block = block.assign(order=block["model"].map(stats.MODEL_ORDER.index)).sort_values(["order", "threads"])
        ns = ns_of(block)
        fig, axes = plt.subplots(len(ns), 2, figsize=(9, 2.6 * len(ns)), squeeze=False)
        for row, n in zip(axes, ns):
            sub = block[block["N"] == n]
            names = [f"{m} T={t}" for m, t in zip(sub["model"], sub["threads"])]
            colors = [COLORS[m] for m in sub["model"]]
            row[0].bar(names, sub["ipc"], color=colors, edgecolor="white", linewidth=1)
            row[0].set_ylabel("IPC (system-wide)")
            row[1].bar(names, sub["stalls_l3_miss_pct"], color=colors, edgecolor="white", linewidth=1)
            row[1].set_ylabel("cycles stalled on L3 miss (%)")
            for ax in row:
                ax.set_title(f"N={n}", loc="left")
                ax.tick_params(axis="x", rotation=40)
        fig.suptitle(f"perf counters, SMT {smt}", x=0.01, ha="left")
        fig.tight_layout()
        save(fig, f"perf_smt{smt}")

    per_cpu = pd.read_csv(stats.RESULTS / "perf_per_cpu.csv")
    for (smt, n), block in per_cpu.groupby(["smt", "N"]):
        configs = block[["model", "threads"]].drop_duplicates()
        configs = configs.assign(order=configs["model"].map(stats.MODEL_ORDER.index)).sort_values(["order", "threads"])
        grid = np.array([[block[(block["model"] == m) & (block["threads"] == t) & (block["cpu"] == c)]["busy_share_pct"].sum()
                          for c in sorted(block["cpu"].unique())] for m, t in zip(configs["model"], configs["threads"])])
        fig, ax = plt.subplots(figsize=(6, 0.4 * len(configs) + 1.2))
        image = ax.imshow(grid, cmap="Blues", vmin=0, vmax=100, aspect="auto")
        ax.set_yticks(range(len(configs)), [f"{m} T={t}" for m, t in zip(configs["model"], configs["threads"])])
        ax.set_xticks(range(grid.shape[1]), [f"cpu{c}" for c in sorted(block["cpu"].unique())])
        ax.grid(False)
        fig.colorbar(image, label="cycles, % of busiest logical CPU")
        ax.set_title(f"Work per logical CPU, N={n}, SMT {smt}", loc="left")
        fig.tight_layout()
        save(fig, f"perf_per_cpu_N{n}_smt{smt}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", nargs="*", help="bench CSVs (default: data/bench_*.csv)")
    args = parser.parse_args()

    runs = stats.load_runs(args.csv)
    runs = runs[runs["throttled"] == 0]
    stats.RESULTS.mkdir(exist_ok=True)
    summary = stats.summarise(runs)
    conv = stats.convergence(runs)

    for smt in sorted(runs["smt"].unique()):
        boxplots(runs, smt)
        histograms(runs, smt)
        scaling(summary, smt, "speedup", "speedup vs baseline", "speedup", reference=1)
        scaling(summary, smt, "efficiency", "efficiency (speedup / cores)", "efficiency", reference=1)
    smt_on_off(summary)
    convergence_plot(conv)
    perf_plots()


if __name__ == "__main__":
    main()
