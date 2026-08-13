# ---
# jupyter:
#   jupytext:
#     formats: ipynb,py:percent
#     text_representation:
#       extension: .py
#       format_name: percent
#       format_version: '1.3'
#   kernelspec:
#     display_name: Python 3 (ipykernel)
#     language: python
#     name: python3
# ---

# %% [markdown]
# # Exynos 5422 — Migration Impact on Unbounded Tasksets
#
# Run both allocators (FFCap Adaptive Linear and FFCap u_target=1.0)
# with and without migration on the existing unbounded tasksets.
# Measure:
# - Migration rate per task arrival
# - Acceptance rate difference (migration vs no migration)

# %%
import os
import sys
import random
import multiprocessing as mp
from pathlib import Path
from functools import partial

sys.path.insert(0, str(Path("build/python").resolve()))

import pyschedsim as sim
import polars as pl
import numpy as np
from scipy import stats

MAX_WORKERS = max(1, (os.cpu_count() or 1) - 1)
_mp_ctx = mp.get_context("fork")
print(f"Using {MAX_WORKERS} worker processes")

# %%
PLATFORM = "platforms/exynos5422.json"
TASKSET_DIR_UNBOUNDED = "tasksets_unbounded"
N_SCENARIOS_UNBOUNDED = 1000
N_TASKS_UNBOUNDED = 15
SUCCESS_RATE = 0.5
COMPRESSION_RATE = 0.5
NUM_HYPERPERIODS = 100
UTIL_PER_TASK_MIN = 0.001
UTIL_UNBOUNDED_LO = UTIL_PER_TASK_MIN
UTIL_UNBOUNDED_HI = 0.9

UTIL_MAX = 5.5
UTIL_MIN_UNBOUNDED = 0.1
UTIL_STEP_UNBOUNDED = 0.1
util_points_unbounded = np.round(
    np.arange(UTIL_MIN_UNBOUNDED, UTIL_MAX + UTIL_STEP_UNBOUNDED / 2, UTIL_STEP_UNBOUNDED), 4
)

# %% [markdown]
# ## Rebuild taskset index from existing files

# %%
def uunifast_discard(n, u_total, umin=0.0, umax=1.0, max_attempts=1_000_000):
    """UUniFast-Discard: generate n utilizations summing to u_total with each in [umin, umax]."""
    for _ in range(max_attempts):
        utils = []
        sum_u = u_total
        valid = True
        for i in range(n - 1):
            exp = 1.0 / (n - i - 1)
            next_sum = sum_u * random.random() ** exp
            u = sum_u - next_sum
            if u < umin or u > umax:
                valid = False
                break
            utils.append(u)
            sum_u = next_sum
        if valid and umin <= sum_u <= umax:
            utils.append(sum_u)
            return utils
    raise RuntimeError(
        f"uunifast_discard: no valid split after {max_attempts} attempts "
        f"(n={n}, u_total={u_total}, umin={umin}, umax={umax})")


def _generate_one_unbounded(util, scenario_idx):
    seed = int(round(util * 10000)) * 20000 + scenario_idx
    random.seed(seed)
    utils = uunifast_discard(N_TASKS_UNBOUNDED, util, UTIL_UNBOUNDED_LO, UTIL_UNBOUNDED_HI)
    folder = os.path.join(TASKSET_DIR_UNBOUNDED, str(int(round(util * 10))))
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, f"{scenario_idx + 1}.json")
    if not os.path.exists(path):
        scenario = sim.from_utilizations(utils, SUCCESS_RATE, COMPRESSION_RATE, seed, NUM_HYPERPERIODS, 0)
        sim.write_scenario(scenario, path)
    return {
        "total_util": round(float(util), 4),
        "seed": seed,
        "utilizations": [float(u) for u in utils],
        "path": path,
    }


gen_args = [(util, idx) for util in util_points_unbounded for idx in range(N_SCENARIOS_UNBOUNDED)]
with _mp_ctx.Pool(MAX_WORKERS) as pool:
    tasksets_unbounded = pool.starmap(_generate_one_unbounded, gen_args)

print(f"Indexed {len(tasksets_unbounded)} unbounded tasksets")

# %% [markdown]
# ## Simulation helper

# %%
def run_allocator_variant(taskset: dict, adaptive: bool, platform_path: str = PLATFORM,
                          migration: bool = False) -> dict:
    """Run either FF Cap (u_target=1.0) or the adaptive linear allocator on one taskset."""
    engine = sim.Engine()
    sim.load_platform(engine, platform_path)
    scenario = sim.load_scenario(taskset["path"])

    tasks = sim.inject_scenario(engine, scenario)
    for i, task in enumerate(tasks):
        sim.schedule_arrivals(engine, task, scenario.tasks[i].jobs)
    engine.platform.finalize()

    platform = engine.platform
    ref_freq_max = max(
        platform.clock_domain(i).freq_max
        for i in range(platform.clock_domain_count)
    )

    clusters = []
    for i in range(platform.clock_domain_count):
        cd = platform.clock_domain(i)
        procs = cd.get_processors()
        if not procs:
            continue
        sched = sim.EdfScheduler(engine, procs)
        sched.enable_grub()
        sched.set_admission_test(sim.AdmissionTest.GFB)
        perf = procs[0].type().performance
        cluster = sim.Cluster(cd, sched, perf, ref_freq_max)
        if perf < 1.0:
            cluster.set_u_target(1.0)
        clusters.append(cluster)

    if adaptive:
        allocator = sim.FFCapAdaptiveLinearAllocator(engine, clusters)
        allocator.set_expected_total_util(taskset["total_util"])
        allocator_name = "ff_cap_adaptive_linear"
    else:
        allocator = sim.FFCapAllocator(engine, clusters)
        allocator_name = "ff_cap_u_target_1_0"

    if migration:
        allocator.enable_migration()

    writer = sim.MemoryTraceWriter()
    engine.set_trace_writer(writer)
    engine.run()

    metrics = writer.compute_metrics()
    total_arrivals = metrics.total_jobs + metrics.rejected_arrivals

    sorted_clusters = sorted(clusters, key=lambda c: c.perf)

    return {
        "total_util": taskset["total_util"],
        "seed": taskset["seed"],
        "allocator": allocator_name,
        "migration": migration,
        "total_jobs": metrics.total_jobs,
        "completed_jobs": metrics.completed_jobs,
        "deadline_misses": metrics.deadline_misses,
        "rejected_arrivals": metrics.rejected_arrivals,
        "total_arrivals": total_arrivals,
        "cluster_migrations": metrics.cluster_migrations,
        "little_util": sorted_clusters[0].utilization if len(sorted_clusters) > 0 else 0.0,
        "big_util": sorted_clusters[-1].utilization if len(sorted_clusters) > 1 else 0.0,
    }

# %% [markdown]
# ## Run all 4 combinations: (allocator) x (migration on/off)

# %%
comparison_args = [
    (taskset, is_adaptive, PLATFORM, mig)
    for _, is_adaptive in [("ff_cap_adaptive_linear", True), ("ff_cap_u_target_1_0", False)]
    for mig in [False, True]
    for taskset in tasksets_unbounded
]

print(f"Running {len(comparison_args)} simulations...")
with _mp_ctx.Pool(MAX_WORKERS) as pool:
    comparison_rows = pool.starmap(run_allocator_variant, comparison_args)
print(f"Done: {len(comparison_rows)} runs complete")

df = pl.DataFrame(comparison_rows)

# %% [markdown]
# ## Acceptance rate: with vs without migration

# %%
per_scenario = df.with_columns(
    ((pl.col("total_arrivals") - pl.col("rejected_arrivals"))
     / pl.col("total_arrivals")).alias("acceptance_rate"),
    (pl.col("cluster_migrations") / pl.col("total_arrivals")).alias("migration_rate"),
)

label_map = {
    "ff_cap_adaptive_linear": "FFCap Adaptive Linear",
    "ff_cap_u_target_1_0": "FFCap (u_target = 1.0)",
}
mig_styles = {False: ("-", "o", ""), True: ("--", "s", " + migration")}

summary = (
    df.group_by(["allocator", "migration", "total_util"])
    .agg(
        acceptance_rate=1.0
        - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum(),
        migration_rate=pl.col("cluster_migrations").sum() / pl.col("total_arrivals").sum(),
    )
    .sort(["allocator", "migration", "total_util"])
)

# %%
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(10, 5))
for name, label in label_map.items():
    for mig, (ls, marker, suffix) in mig_styles.items():
        data = summary.filter(
            (pl.col("allocator") == name) & (pl.col("migration") == mig)
        ).sort("total_util")
        ax.plot(
            data["total_util"], data["acceptance_rate"],
            marker=marker, linewidth=1.5, linestyle=ls, markersize=4,
            label=f"{label}{suffix}",
        )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Acceptance Rate")
ax.set_title("Acceptance Rate — With vs Without Migration\nExynos 5422 — Unbounded tasksets")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ## Migration rate per task arrival

# %%
fig, ax = plt.subplots(figsize=(10, 5))
for name, label in label_map.items():
    data = summary.filter(
        (pl.col("allocator") == name) & (pl.col("migration") == True)
    ).sort("total_util")
    ax.plot(
        data["total_util"], data["migration_rate"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Migrations / Total Arrivals")
ax.set_title("Migration Rate per Task Arrival\nExynos 5422 — Unbounded tasksets (migration enabled)")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ## Acceptance rate difference: migration − no migration
#
# Per-scenario paired difference to measure the benefit of enabling migration.

# %%
def sig_label(p: float) -> str:
    if p < 0.001:
        return "***"
    if p < 0.01:
        return "**"
    if p < 0.05:
        return "*"
    return "ns"


SIG_COLORS = {"***": "darkgreen", "**": "seagreen", "*": "goldenrod", "ns": "lightgray"}

# %%
from matplotlib.patches import Patch

for alloc_key, alloc_label in label_map.items():
    sub = per_scenario.filter(pl.col("allocator") == alloc_key)

    no_mig = (
        sub.filter(pl.col("migration") == False)
        .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_no_mig")])
    )
    with_mig = (
        sub.filter(pl.col("migration") == True)
        .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_mig")])
    )
    paired = (
        with_mig.join(no_mig, on=["total_util", "seed"])
        .with_columns((pl.col("acc_mig") - pl.col("acc_no_mig")).alias("gain"))
        .sort(["total_util", "seed"])
    )

    sig_rows = []
    for util in sorted(paired["total_util"].unique().to_list()):
        gains = paired.filter(pl.col("total_util") == util)["gain"].to_numpy()
        mean_gain = float(gains.mean())
        if np.all(gains == 0):
            p_val = 1.0
        else:
            _, p_val = stats.wilcoxon(gains, alternative="two-sided")
        sig_rows.append({
            "total_util": util,
            "mean_gain_pct": round(mean_gain * 100, 4),
            "p_value": float(p_val),
            "significance": sig_label(p_val),
        })

    sig_df = pl.DataFrame(sig_rows)

    utils_arr = sig_df["total_util"].to_numpy()
    gains_arr = sig_df["mean_gain_pct"].to_numpy()
    sig_labels = sig_df["significance"].to_list()

    fig, ax = plt.subplots(figsize=(12, 5))
    colors = [SIG_COLORS[s] for s in sig_labels]
    bars = ax.bar(
        [f"{u:.1f}" for u in utils_arr], gains_arr,
        color=colors, edgecolor="black", linewidth=0.5,
    )

    for bar, label in zip(bars, sig_labels):
        y = bar.get_height()
        va = "bottom" if y >= 0 else "top"
        offset = 0.15 if y >= 0 else -0.15
        ax.text(bar.get_x() + bar.get_width() / 2, y + offset, label,
                ha="center", va=va, fontsize=6, fontweight="bold")

    ax.axhline(0, color="black", linewidth=0.8)
    ax.set_xlabel("Total Utilization")
    ax.set_ylabel("Mean Acceptance Rate Gain (%)")
    ax.set_title(f"Acceptance Rate Gain (Migration − No Migration)\n{alloc_label}")
    ax.tick_params(axis="x", rotation=45)
    ax.grid(True, axis="y", alpha=0.3)

    legend_elements = [
        Patch(facecolor="darkgreen", edgecolor="black", label="p < 0.001 (***)"),
        Patch(facecolor="seagreen", edgecolor="black", label="p < 0.01 (**)"),
        Patch(facecolor="goldenrod", edgecolor="black", label="p < 0.05 (*)"),
        Patch(facecolor="lightgray", edgecolor="black", label="not significant (ns)"),
    ]
    ax.legend(handles=legend_elements, fontsize=8, loc="upper left")
    plt.tight_layout()
    plt.show()

    print(f"\n{alloc_label} — Migration gain summary:")
    print(sig_df)

# %% [markdown]
# ## Export CSV

# %%
OUTDIR = os.path.expanduser("~/Nextcloud/these/manuscript/chap5-dynamic-allocation/data")
os.makedirs(OUTDIR, exist_ok=True)

# Acceptance rate + migration rate per (allocator, migration, total_util)
summary.write_csv(os.path.join(OUTDIR, "exynos_migration_acceptance.csv"))
print(f"exynos_migration_acceptance.csv: {summary.shape[0]} rows")

# Per-allocator migration gain significance
all_sig_dfs = []
for alloc_key, alloc_label in label_map.items():
    sub = per_scenario.filter(pl.col("allocator") == alloc_key)
    no_mig = (
        sub.filter(pl.col("migration") == False)
        .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_no_mig")])
    )
    with_mig = (
        sub.filter(pl.col("migration") == True)
        .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_mig")])
    )
    paired = (
        with_mig.join(no_mig, on=["total_util", "seed"])
        .with_columns((pl.col("acc_mig") - pl.col("acc_no_mig")).alias("gain"))
        .sort(["total_util", "seed"])
    )

    sig_rows = []
    for util in sorted(paired["total_util"].unique().to_list()):
        gains = paired.filter(pl.col("total_util") == util)["gain"].to_numpy()
        mean_gain = float(gains.mean())
        if np.all(gains == 0):
            p_val = 1.0
        else:
            _, p_val = stats.wilcoxon(gains, alternative="two-sided")
        sig_rows.append({
            "total_util": util,
            "allocator": alloc_key,
            "mean_gain_pct": round(mean_gain * 100, 4),
            "p_value": float(p_val),
            "significance": sig_label(p_val),
        })
    all_sig_dfs.append(pl.DataFrame(sig_rows))

sig_export = pl.concat(all_sig_dfs)
sig_export.write_csv(os.path.join(OUTDIR, "exynos_migration_sig.csv"))
print(f"exynos_migration_sig.csv: {sig_export.shape[0]} rows")

print(f"\nExported to {OUTDIR}/")

# %% [markdown]
# ## Summary table

# %%
print("\n=== Aggregated summary ===\n")
print(summary.select(["allocator", "migration", "total_util", "acceptance_rate", "migration_rate"]))
