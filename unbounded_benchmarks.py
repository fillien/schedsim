# ---
# jupyter:
#   jupytext:
#     formats: ipynb,py:percent
#     text_representation:
#       extension: .py
#       format_name: percent
#       format_version: '1.3'
#       jupytext_version: 1.17.3
#   kernelspec:
#     display_name: Python 3 (ipykernel)
#     language: python
#     name: python3
# ---

# %% [markdown] editable=true slideshow={"slide_type": ""}
# # Allocator Benchmarks on Unbounded Tasksets
#
# Compare FFCap Adaptive Linear vs FFCap (u_target = 1.0) and
# evaluate energy consumption under different DVFS/DPM policies
# on unbounded and mixed-profile tasksets.

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
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import numpy as np
from scipy import stats
from IPython.display import clear_output

MAX_WORKERS = max(1, (os.cpu_count() or 1) - 1)
_mp_ctx = mp.get_context("fork")
print(f"Using {MAX_WORKERS} worker processes")

# Enable autoreload so local module changes propagate in notebooks.
try:
    get_ipython().run_line_magic("load_ext", "autoreload")
    get_ipython().run_line_magic("autoreload", "2")
except Exception:
    pass

# %%
PLATFORM = "platforms/exynos5422.json"
N_SCENARIOS = 100
SUCCESS_RATE = 0.5
COMPRESSION_RATE = 0.5
NUM_HYPERPERIODS = 100  # repeat job pattern over 100 hyperperiods for longer simulations
MIN_JOBS = 700  # minimum total jobs per scenario
UTIL_PER_TASK_MIN = 0.001
UTIL_PER_TASK_MAX = 0.133

# Exynos 5422 capacity: 4 big (perf=1.0) + 4 LITTLE (perf=0.3334)
BIG_CAP = 4 * 1.0
LITTLE_CAP = 4 * 0.3334
PLATFORM_CAP = BIG_CAP + LITTLE_CAP  # 5.3336

UTIL_MIN = 0.0
UTIL_MAX = 5.5
UTIL_STEP = 0.2

# Need enough tasks so that N_TASKS * UTIL_PER_TASK_MAX >= UTIL_MAX
N_TASKS = int(np.ceil(UTIL_MAX / UTIL_PER_TASK_MAX))  # 49
print(f"N_TASKS = {N_TASKS}  (per-task util in [{UTIL_PER_TASK_MIN}, {UTIL_PER_TASK_MAX}])")

util_points = np.round(np.arange(UTIL_MIN, UTIL_MAX + UTIL_STEP / 2, UTIL_STEP), 4)


# %% [markdown]
# ## Simulation helpers

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

    # Per-cluster utilization sorted by perf ascending (stable sort: for equal-perf
    # clusters like Orion big/mid, clock domain construction order is preserved).
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
        "cluster_utils": [c.utilization for c in sorted_clusters],
        "little_util": sorted_clusters[0].utilization if len(sorted_clusters) > 0 else 0.0,
        "big_util": sorted_clusters[-1].utilization if len(sorted_clusters) > 1 else 0.0,
    }


# %% [markdown]
# ## Unbounded taskset generation
#
# Generate tasksets with a fixed number of tasks (N_TASKS_UNBOUNDED)
# using UUniFast-Discard with per-task utilization in
# [UTIL_UNBOUNDED_LO, UTIL_UNBOUNDED_HI].

# %%
TASKSET_DIR_UNBOUNDED = "tasksets_unbounded"
N_SCENARIOS_UNBOUNDED = 1000
N_TASKS_UNBOUNDED = 15
UTIL_MIN_UNBOUNDED = 0.1
UTIL_STEP_UNBOUNDED = 0.1
UTIL_UNBOUNDED_LO = UTIL_PER_TASK_MIN
UTIL_UNBOUNDED_HI = 0.9
util_points_unbounded = np.round(
    np.arange(UTIL_MIN_UNBOUNDED, UTIL_MAX + UTIL_STEP_UNBOUNDED / 2, UTIL_STEP_UNBOUNDED), 4
)

def _generate_one_unbounded(util, scenario_idx):
    seed = int(round(util * 10000)) * 20000 + scenario_idx
    random.seed(seed)
    utils = uunifast_discard(N_TASKS_UNBOUNDED, util, UTIL_UNBOUNDED_LO, UTIL_UNBOUNDED_HI)
    folder = os.path.join(TASKSET_DIR_UNBOUNDED, str(int(round(util * 10))))
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, f"{scenario_idx + 1}.json")
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

print(f"Generated {len(tasksets_unbounded)} unbounded tasksets in {TASKSET_DIR_UNBOUNDED}/")

# %% [markdown]
# ### Unbounded utilization distribution

# %%
sample_points_unbounded = [2.0, 3.0, 5.0, round(UTIL_MAX, 1)]
fig, axes = plt.subplots(1, len(sample_points_unbounded), figsize=(16, 4), sharey=True)

for ax, target in zip(axes, sample_points_unbounded):
    target_r = round(target, 4)
    all_utils = [u for t in tasksets_unbounded if t["total_util"] == target_r for u in t["utilizations"]]
    ax.hist(all_utils, bins=20, edgecolor="black", alpha=0.7)
    ax.set_title(f"$U_{{total}}$ = {target:.1f}")
    ax.set_xlabel("Per-task utilization")

axes[0].set_ylabel("Count")
fig.suptitle("Distribution of per-task utilizations (DRS, unbounded)", fontsize=13)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Job duration distribution
#
# Show the distribution of actual job durations for one task in a sample
# taskset, compared to the task's WCET, to illustrate the Weibull-based
# execution time model.

# %%
import json

sample_taskset = tasksets_unbounded[0]
with open(sample_taskset["path"]) as f:
    scenario_json = json.load(f)

task_data = scenario_json["tasks"][0]
durations = np.array([j["duration"] for j in task_data["jobs"]])
wcet = task_data["wcet"]
period = task_data["period"]

fig, ax = plt.subplots(figsize=(8, 4))
ax.hist(durations * 1e6, bins=30, edgecolor="black", alpha=0.7)
ax.axvline(wcet * 1e6, color="red", linestyle="--", linewidth=1.5, label=f"WCET = {wcet*1e6:.1f} µs")
ax.set_xlabel("Job duration (µs)")
ax.set_ylabel("Count")
ax.set_title(
    f"Job duration distribution — Task {task_data['id']} "
    f"(T = {period*1e6:.0f} µs, U = {sample_taskset['utilizations'][0]:.3f})"
)
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ## Adaptive vs FFCap (u_target = 1.0) — unbounded tasksets
#
# Compare the acceptance rate of the adaptive allocator against classic
# FFCap with a permissive LITTLE-cluster threshold (`u_target = 1.0`)
# on the unbounded tasksets.

# %%
comparison_args = [
    (taskset, is_adaptive)
    for _, is_adaptive in [("ff_cap_adaptive_linear", True), ("ff_cap_u_target_1_0", False)]
    for taskset in tasksets_unbounded
]

with _mp_ctx.Pool(MAX_WORKERS) as pool:
    comparison_rows = pool.starmap(run_allocator_variant, comparison_args)
print(f"Allocator comparison: {len(comparison_rows)} runs complete")

comparison_df = pl.DataFrame(comparison_rows)

# %%
# Per-scenario acceptance rate: accepted / total_arrivals for each (seed, total_util)
comparison_per_scenario = (
    comparison_df.with_columns(
        (pl.col("total_arrivals") - pl.col("rejected_arrivals"))
        .truediv(pl.col("total_arrivals"))
        .alias("acceptance_rate")
    )
)

# Pivot so we have one row per scenario with both allocators side by side
adaptive_sc = (
    comparison_per_scenario
    .filter(pl.col("allocator") == "ff_cap_adaptive_linear")
    .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_adaptive")])
)
baseline_sc = (
    comparison_per_scenario
    .filter(pl.col("allocator") == "ff_cap_u_target_1_0")
    .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_baseline")])
)
paired = (
    adaptive_sc.join(baseline_sc, on=["total_util", "seed"])
    .with_columns((pl.col("acc_adaptive") - pl.col("acc_baseline")).alias("gain"))
    .sort(["total_util", "seed"])
)

# %% [markdown]
# ### Statistical significance (Wilcoxon signed-rank test)
#
# For each utilization point, test whether the adaptive allocator's
# per-scenario acceptance rate differs significantly from FFCap.

# %%
def sig_label(p: float) -> str:
    if p < 0.001:
        return "***"
    if p < 0.01:
        return "**"
    if p < 0.05:
        return "*"
    return "ns"


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
sig_df

# %%
label_map = {
    "ff_cap_adaptive_linear": "FFCap Adaptive Linear",
    "ff_cap_u_target_1_0": "FFCap (u_target = 1.0)",
}

comparison_summary = (
    comparison_df.group_by(["allocator", "total_util"])
    .agg(
        acceptance_rate=1.0
        - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum(),
    )
    .sort(["allocator", "total_util"])
)

fig, ax = plt.subplots(figsize=(10, 5))
for name, label in label_map.items():
    data = comparison_summary.filter(pl.col("allocator") == name).sort("total_util")
    ax.plot(
        data["total_util"], data["acceptance_rate"],
        marker="o", linewidth=1.5, label=label,
    )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Acceptance Rate")
ax.set_title("Acceptance Rate — Adaptive vs FFCap (u_target = 1.0)\nUnbounded tasksets")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Deadline misses per allocator

# %%
deadline_summary = (
    comparison_df.group_by(["allocator", "total_util"])
    .agg(
        deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum(),
    )
    .sort(["allocator", "total_util"])
)

fig, ax = plt.subplots(figsize=(10, 5))
for name, label in label_map.items():
    data = deadline_summary.filter(pl.col("allocator") == name).sort("total_util")
    ax.plot(
        data["total_util"], data["deadline_miss_rate"],
        marker="o", linewidth=1.5, label=label,
    )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Deadline Miss Rate")
ax.set_ylim(0, 1)
ax.set_title("Deadline Miss Rate — Adaptive vs FFCap (u_target = 1.0)\nUnbounded tasksets")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Effective completion rate
#
# Fraction of all arriving jobs that were both admitted and completed
# before their deadline: `completed_jobs / total_arrivals`.

# %%
completion_summary = (
    comparison_df.group_by(["allocator", "total_util"])
    .agg(
        completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum(),
    )
    .sort(["allocator", "total_util"])
)

fig, ax = plt.subplots(figsize=(10, 5))
for name, label in label_map.items():
    data = completion_summary.filter(pl.col("allocator") == name).sort("total_util")
    ax.plot(
        data["total_util"], data["completion_rate"],
        marker="o", linewidth=1.5, label=label,
    )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Effective Completion Rate")
ax.set_ylim(0, 1)
ax.set_title("Effective Completion Rate — Adaptive vs FFCap (u_target = 1.0)\nUnbounded tasksets")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Per-cluster utilization
#
# Average utilization admitted to the LITTLE and big clusters for each
# allocator, showing how task placement differs.

# %%
cluster_summary = (
    comparison_df.group_by(["allocator", "total_util"])
    .agg(
        mean_little_util=pl.col("little_util").mean(),
        mean_big_util=pl.col("big_util").mean(),
    )
    .sort(["allocator", "total_util"])
)

fig, axes = plt.subplots(1, 2, figsize=(14, 5))

for name, label in label_map.items():
    data = cluster_summary.filter(pl.col("allocator") == name).sort("total_util")
    axes[0].plot(
        data["total_util"], data["mean_little_util"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

axes[0].set_xlabel("Total Utilization")
axes[0].set_ylabel("Mean Admitted Utilization")
axes[0].set_title("LITTLE Cluster — Admitted Utilization")
axes[0].legend()
axes[0].grid(True, alpha=0.3)

for name, label in label_map.items():
    data = cluster_summary.filter(pl.col("allocator") == name).sort("total_util")
    axes[1].plot(
        data["total_util"], data["mean_big_util"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

axes[1].set_xlabel("Total Utilization")
axes[1].set_ylabel("Mean Admitted Utilization")
axes[1].set_title("Big Cluster — Admitted Utilization")
axes[1].legend()
axes[1].grid(True, alpha=0.3)

fig.suptitle("Per-Cluster Admitted Utilization — Adaptive vs FFCap", fontsize=13)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Acceptance rate gain (Adaptive − FFCap)

# %%
utils_arr = sig_df["total_util"].to_numpy()
gains_arr = sig_df["mean_gain_pct"].to_numpy()
sig_labels = sig_df["significance"].to_list()

SIG_COLORS = {"***": "darkgreen", "**": "seagreen", "*": "goldenrod", "ns": "lightgray"}

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
ax.set_title("Acceptance Rate Gain (Adaptive − FFCap)")
ax.tick_params(axis="x", rotation=45)
ax.grid(True, axis="y", alpha=0.3)

from matplotlib.patches import Patch
legend_elements = [
    Patch(facecolor="darkgreen", edgecolor="black", label="p < 0.001 (***)"),
    Patch(facecolor="seagreen", edgecolor="black", label="p < 0.01 (**)"),
    Patch(facecolor="goldenrod", edgecolor="black", label="p < 0.05 (*)"),
    Patch(facecolor="lightgray", edgecolor="black", label="not significant (ns)"),
]
ax.legend(handles=legend_elements, fontsize=8, loc="upper left")
plt.tight_layout()
plt.show()

# %% [markdown]
# ## Energy consumption — CSF vs FFA
#
# Compare energy consumption of the CSF (Core Scaling First) and FFA
# (Feedback-based Frequency Adaptation) DVFS/DPM policies on the
# unbounded tasksets, using FFCap allocation with `u_target = 1.0`.

# %%
def run_energy_variant(taskset: dict, dvfs_policy: str, platform_path: str = PLATFORM,
                       migration: bool = False) -> dict:
    """Run FFCap with a DVFS/DPM policy and return energy metrics."""
    engine = sim.Engine()
    sim.load_platform(engine, platform_path)
    engine.enable_energy_tracking(True)
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
        if dvfs_policy == "csf":
            sched.enable_csf(cooldown=0.0)
        elif dvfs_policy == "ffa":
            sched.enable_ffa(cooldown=0.0)
        elif dvfs_policy == "grub_pa":
            sched.enable_power_aware_dvfs(cooldown=0.0)
        elif dvfs_policy == "grub":
            pass  # baseline: no DVFS, no DPM
        perf = procs[0].type().performance
        cluster = sim.Cluster(cd, sched, perf, ref_freq_max)
        if perf < 1.0:
            cluster.set_u_target(1.0)
        clusters.append(cluster)

    allocator = sim.FFCapAdaptiveLinearAllocator(engine, clusters)
    allocator.set_expected_total_util(taskset["total_util"])

    if migration:
        allocator.enable_migration()

    writer = sim.MemoryTraceWriter()
    engine.set_trace_writer(writer)
    engine.run()

    sim_duration_s = engine.time
    metrics = writer.compute_metrics()
    total_arrivals = metrics.total_jobs + metrics.rejected_arrivals
    avg_power_mw = metrics.total_energy_mj / sim_duration_s if sim_duration_s > 0 else 0.0

    return {
        "total_util": taskset["total_util"],
        "seed": taskset["seed"],
        "policy": dvfs_policy,
        "migration": migration,
        "total_energy_mj": metrics.total_energy_mj,
        "sim_duration_s": sim_duration_s,
        "avg_power_mw": avg_power_mw,
        "total_jobs": metrics.total_jobs,
        "completed_jobs": metrics.completed_jobs,
        "deadline_misses": metrics.deadline_misses,
        "rejected_arrivals": metrics.rejected_arrivals,
        "total_arrivals": total_arrivals,
        "cluster_migrations": metrics.cluster_migrations,
    }


# %%
ENERGY_POLICIES = [("grub", "GRUB"), ("grub_pa", "GRUB-PA"), ("csf", "CSF"), ("ffa", "FFA")]

energy_args = [
    (taskset, policy_key)
    for policy_key, _ in ENERGY_POLICIES
    for taskset in tasksets_unbounded
]

with _mp_ctx.Pool(MAX_WORKERS) as pool:
    energy_rows = pool.starmap(run_energy_variant, energy_args)
print(f"Energy benchmark: {len(energy_rows)} runs complete")

energy_df = pl.DataFrame(energy_rows)

# %%
energy_summary = (
    energy_df.group_by(["policy", "total_util"])
    .agg(
        mean_avg_power_mw=pl.col("avg_power_mw").mean(),
        acceptance_rate=1.0
        - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum(),
        deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum(),
        completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum(),
    )
    .sort(["policy", "total_util"])
)
energy_summary

# %%
policy_labels = {"grub": "GRUB", "grub_pa": "GRUB-PA", "csf": "CSF", "ffa": "FFA"}

fig, axes = plt.subplots(1, 4, figsize=(22, 5))

# Average power consumption
for policy_key, label in policy_labels.items():
    data = energy_summary.filter(pl.col("policy") == policy_key).sort("total_util")
    axes[0].plot(
        data["total_util"], data["mean_avg_power_mw"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

axes[0].set_xlabel("Total Utilization")
axes[0].set_ylabel("Mean Average Power (mW)")
axes[0].set_title("Average Power Consumption")
axes[0].legend()
axes[0].grid(True, alpha=0.3)

# Acceptance rate
for policy_key, label in policy_labels.items():
    data = energy_summary.filter(pl.col("policy") == policy_key).sort("total_util")
    axes[1].plot(
        data["total_util"], data["acceptance_rate"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

axes[1].set_xlabel("Total Utilization")
axes[1].set_ylabel("Acceptance Rate")
axes[1].set_title("Acceptance Rate")
axes[1].legend()
axes[1].grid(True, alpha=0.3)

# Deadline misses
for policy_key, label in policy_labels.items():
    data = energy_summary.filter(pl.col("policy") == policy_key).sort("total_util")
    axes[2].plot(
        data["total_util"], data["deadline_miss_rate"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

axes[2].set_xlabel("Total Utilization")
axes[2].set_ylabel("Deadline Miss Rate")
axes[2].set_ylim(0, 1)
axes[2].set_title("Deadline Miss Rate")
axes[2].legend()
axes[2].grid(True, alpha=0.3)

# Effective completion rate
for policy_key, label in policy_labels.items():
    data = energy_summary.filter(pl.col("policy") == policy_key).sort("total_util")
    axes[3].plot(
        data["total_util"], data["completion_rate"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

axes[3].set_xlabel("Total Utilization")
axes[3].set_ylabel("Effective Completion Rate")
axes[3].set_ylim(0, 1)
axes[3].set_title("Effective Completion Rate")
axes[3].legend()
axes[3].grid(True, alpha=0.3)

fig.suptitle("Exynos 5422 — Energy Benchmark (unbounded tasksets)", fontsize=13)
plt.tight_layout()
plt.show()

# %% [markdown]
# ## Mixed-profile taskset generation
#
# Generate tasksets composed of three utilization classes to model a
# heterogeneous workload:
#
# | Share | Utilization range | Profile |
# |-------|-------------------|---------|
# | 40 %  | [0.01, 0.12]      | Light   |
# | 20 %  | [0.25, 0.33]      | Medium  |
# | 40 %  | [0.50, 0.80]      | Heavy   |
#
# For each total utilization target, the budget is distributed across
# classes proportionally to `n_class × midpoint`, then clamped to
# feasible bounds and generated with DRS.

# %%
TASKSET_DIR_MIXED = "tasksets_mixed"
N_SCENARIOS_MIXED = 1000

# (fraction of tasks, lower bound, upper bound)
UTIL_CLASSES = [
    (0.40, 0.01, 0.12),  # light
    (0.20, 0.25, 0.33),  # medium
    (0.40, 0.50, 0.80),  # heavy
]

CLASS_MIDS = [(lo + hi) / 2 for _, lo, hi in UTIL_CLASSES]
FRAC_MID_SUM = sum(f * m for (f, _, _), m in zip(UTIL_CLASSES, CLASS_MIDS))


def compute_class_counts(total_util: float) -> list[int]:
    """Compute per-class task counts so the per-task average ≈ class midpoint.

    This gives DRS the best conditions for approximately uniform marginals.
    If the resulting counts can't accommodate total_util, the heavy class
    count is increased until feasible.
    """
    counts = [max(1, round(total_util * f / FRAC_MID_SUM)) for f, _, _ in UTIL_CLASSES]
    while sum(nc * hi for nc, (_, _, hi) in zip(counts, UTIL_CLASSES)) < total_util:
        counts[-1] += 1
    return counts


for util in [UTIL_MIN, (UTIL_MIN + UTIL_MAX) / 2, UTIL_MAX]:
    cc = compute_class_counts(util)
    print(f"U={util:.1f} → {sum(cc)} tasks: {cc}")
    for (frac, lo, hi), nc in zip(UTIL_CLASSES, cc):
        print(f"    {nc} tasks in [{lo}, {hi}] ({frac*100:.0f}%)")


# %%
def generate_mixed_utilizations(
    total_util: float,
    seed: int,
) -> list[float]:
    """Generate per-task utilizations for a mixed-profile taskset.

    The number of tasks per class is chosen so that the per-task average
    matches the class midpoint, giving DRS approximately uniform marginals.
    """
    random.seed(seed)
    cc = compute_class_counts(total_util)

    # Distribute total_util so per-task average ≈ class midpoint
    weights = [nc * mid for nc, mid in zip(cc, CLASS_MIDS)]
    total_weight = sum(weights)
    class_utils = [total_util * w / total_weight for w in weights]

    # Iteratively clamp and redistribute
    for _ in range(20):
        excess = 0.0
        unfixed = []
        for i, (nc, (_, lo, hi)) in enumerate(zip(cc, UTIL_CLASSES)):
            lo_sum, hi_sum = nc * lo, nc * hi
            if class_utils[i] < lo_sum:
                excess += class_utils[i] - lo_sum
                class_utils[i] = lo_sum
            elif class_utils[i] > hi_sum:
                excess += class_utils[i] - hi_sum
                class_utils[i] = hi_sum
            else:
                unfixed.append(i)
        if abs(excess) < 1e-10 or not unfixed:
            break
        for i in unfixed:
            class_utils[i] += excess / len(unfixed)

    # Clamp each class to its feasible range and call DRS
    all_utils = []
    for i, (nc, (_, lo, hi)) in enumerate(zip(cc, UTIL_CLASSES)):
        lo_sum, hi_sum = nc * lo, nc * hi
        class_utils[i] = max(lo_sum, min(hi_sum, class_utils[i]))
        if class_utils[i] <= lo_sum + 1e-10:
            utils = [lo] * nc
        elif class_utils[i] >= hi_sum - 1e-10:
            utils = [hi] * nc
        else:
            utils = uunifast_discard(nc, class_utils[i], lo, hi)
        all_utils.extend(utils)
    return all_utils


UTIL_MIN_MIXED = 1.0
UTIL_MAX_MIXED = 5.5
UTIL_STEP_MIXED = 0.1
util_points_mixed = np.round(
    np.arange(UTIL_MIN_MIXED, UTIL_MAX_MIXED + UTIL_STEP_MIXED / 2, UTIL_STEP_MIXED), 4
)

def _generate_one_mixed(util, scenario_idx):
    seed = int(round(util * 10000)) * 30000 + scenario_idx
    utils = generate_mixed_utilizations(util, seed)
    folder = os.path.join(TASKSET_DIR_MIXED, str(int(round(util * 10))))
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, f"{scenario_idx + 1}.json")
    scenario = sim.from_utilizations(utils, SUCCESS_RATE, COMPRESSION_RATE, seed, NUM_HYPERPERIODS, MIN_JOBS)
    sim.write_scenario(scenario, path)
    return {
        "total_util": round(float(util), 4),
        "seed": seed,
        "utilizations": [float(u) for u in utils],
        "path": path,
    }


mixed_gen_args = [(util, idx) for util in util_points_mixed for idx in range(N_SCENARIOS_MIXED)]
with _mp_ctx.Pool(MAX_WORKERS) as pool:
    tasksets_mixed = pool.starmap(_generate_one_mixed, mixed_gen_args)

print(f"Generated {len(tasksets_mixed)} mixed-profile tasksets in {TASKSET_DIR_MIXED}/")

# %% [markdown]
# ### Mixed-profile utilization distribution

# %%
sample_points_mixed = [2.0, 3.0, 4.0, 5.0]
class_boundaries = [(lo, hi) for _, lo, hi in UTIL_CLASSES]

fig, axes = plt.subplots(1, len(sample_points_mixed), figsize=(16, 4), sharey=True)

for ax, target in zip(axes, sample_points_mixed):
    target_r = round(target, 4)
    all_utils = [
        u for t in tasksets_mixed if t["total_util"] == target_r
        for u in t["utilizations"]
    ]
    ax.hist(all_utils, bins=40, edgecolor="black", alpha=0.7)
    for lo, hi in class_boundaries:
        ax.axvspan(lo, hi, alpha=0.12, color="red")
    ax.set_title(f"$U_{{total}}$ = {target:.1f}")
    ax.set_xlabel("Per-task utilization")

axes[0].set_ylabel("Count")
fig.suptitle(
    "Distribution of per-task utilizations (mixed profile: "
    "40% light, 20% medium, 40% heavy)",
    fontsize=12,
)
plt.tight_layout()
plt.show()

# %% [markdown]
# ## Adaptive vs FFCap (u_target = 1.0) — mixed-profile tasksets
#
# Same comparison as the unbounded section but on the mixed-profile
# tasksets (40 % light / 20 % medium / 40 % heavy).

# %%
mixed_comparison_args = [
    (taskset, is_adaptive)
    for _, is_adaptive in [("ff_cap_adaptive_linear", True), ("ff_cap_u_target_1_0", False)]
    for taskset in tasksets_mixed
]

with _mp_ctx.Pool(MAX_WORKERS) as pool:
    mixed_comparison_rows = pool.starmap(run_allocator_variant, mixed_comparison_args)
print(f"Mixed comparison: {len(mixed_comparison_rows)} runs complete")

mixed_comparison_df = pl.DataFrame(mixed_comparison_rows)

# %%
mixed_per_scenario = (
    mixed_comparison_df.with_columns(
        (pl.col("total_arrivals") - pl.col("rejected_arrivals"))
        .truediv(pl.col("total_arrivals"))
        .alias("acceptance_rate")
    )
)

mixed_adaptive_sc = (
    mixed_per_scenario
    .filter(pl.col("allocator") == "ff_cap_adaptive_linear")
    .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_adaptive")])
)
mixed_baseline_sc = (
    mixed_per_scenario
    .filter(pl.col("allocator") == "ff_cap_u_target_1_0")
    .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_baseline")])
)
mixed_paired = (
    mixed_adaptive_sc.join(mixed_baseline_sc, on=["total_util", "seed"])
    .with_columns((pl.col("acc_adaptive") - pl.col("acc_baseline")).alias("gain"))
    .sort(["total_util", "seed"])
)

# %% [markdown]
# ### Statistical significance (Wilcoxon signed-rank test)

# %%
mixed_sig_rows = []
for util in sorted(mixed_paired["total_util"].unique().to_list()):
    gains = mixed_paired.filter(pl.col("total_util") == util)["gain"].to_numpy()
    mean_gain = float(gains.mean())
    if np.all(gains == 0):
        p_val = 1.0
    else:
        _, p_val = stats.wilcoxon(gains, alternative="two-sided")
    mixed_sig_rows.append({
        "total_util": util,
        "mean_gain_pct": round(mean_gain * 100, 4),
        "p_value": float(p_val),
        "significance": sig_label(p_val),
    })

mixed_sig_df = pl.DataFrame(mixed_sig_rows)
mixed_sig_df

# %%
mixed_label_map = {
    "ff_cap_adaptive_linear": "FFCap Adaptive Linear",
    "ff_cap_u_target_1_0": "FFCap (u_target = 1.0)",
}

mixed_comparison_summary = (
    mixed_comparison_df.group_by(["allocator", "total_util"])
    .agg(
        acceptance_rate=1.0
        - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum(),
    )
    .sort(["allocator", "total_util"])
)

fig, ax = plt.subplots(figsize=(10, 5))
for name, label in mixed_label_map.items():
    data = mixed_comparison_summary.filter(pl.col("allocator") == name).sort("total_util")
    ax.plot(
        data["total_util"], data["acceptance_rate"],
        marker="o", linewidth=1.5, label=label,
    )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Acceptance Rate")
ax.set_title("Acceptance Rate — Adaptive vs FFCap (u_target = 1.0)\nMixed-profile tasksets")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Effective completion rate — mixed profile

# %%
mixed_completion_summary = (
    mixed_comparison_df.group_by(["allocator", "total_util"])
    .agg(
        completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum(),
    )
    .sort(["allocator", "total_util"])
)

fig, ax = plt.subplots(figsize=(10, 5))
for name, label in mixed_label_map.items():
    data = mixed_completion_summary.filter(pl.col("allocator") == name).sort("total_util")
    ax.plot(
        data["total_util"], data["completion_rate"],
        marker="o", linewidth=1.5, label=label,
    )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Effective Completion Rate")
ax.set_ylim(0, 1)
ax.set_title("Effective Completion Rate — Adaptive vs FFCap (u_target = 1.0)\nMixed-profile tasksets")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Acceptance rate gain (Adaptive − FFCap) — mixed profile

# %%
mixed_utils_arr = mixed_sig_df["total_util"].to_numpy()
mixed_gains_arr = mixed_sig_df["mean_gain_pct"].to_numpy()
mixed_sig_labels = mixed_sig_df["significance"].to_list()

SIG_COLORS_MIXED = {"***": "darkgreen", "**": "seagreen", "*": "goldenrod", "ns": "lightgray"}

fig, ax = plt.subplots(figsize=(12, 5))
colors_mixed = [SIG_COLORS_MIXED[s] for s in mixed_sig_labels]
bars = ax.bar(
    [f"{u:.1f}" for u in mixed_utils_arr], mixed_gains_arr,
    color=colors_mixed, edgecolor="black", linewidth=0.5,
)

for bar, label in zip(bars, mixed_sig_labels):
    y = bar.get_height()
    va = "bottom" if y >= 0 else "top"
    offset = 0.15 if y >= 0 else -0.15
    ax.text(bar.get_x() + bar.get_width() / 2, y + offset, label,
            ha="center", va=va, fontsize=6, fontweight="bold")

ax.axhline(0, color="black", linewidth=0.8)
ax.set_xlabel("Total Utilization")
ax.set_ylabel("Mean Acceptance Rate Gain (%)")
ax.set_title("Acceptance Rate Gain (Adaptive − FFCap)\nMixed-profile tasksets")
ax.tick_params(axis="x", rotation=45)
ax.grid(True, axis="y", alpha=0.3)

from matplotlib.patches import Patch
legend_elements_mixed = [
    Patch(facecolor="darkgreen", edgecolor="black", label="p < 0.001 (***)"),
    Patch(facecolor="seagreen", edgecolor="black", label="p < 0.01 (**)"),
    Patch(facecolor="goldenrod", edgecolor="black", label="p < 0.05 (*)"),
    Patch(facecolor="lightgray", edgecolor="black", label="not significant (ns)"),
]
ax.legend(handles=legend_elements_mixed, fontsize=8, loc="upper left")
plt.tight_layout()
plt.show()

# %% [markdown]
# ## Energy consumption — mixed-profile tasksets
#
# Same DVFS/DPM comparison (GRUB, GRUB-PA, CSF, FFA) but on the
# mixed-profile tasksets, which have a controlled number of tasks and
# bounded per-task utilization at every utilization point.

# %%
mixed_energy_args = [
    (taskset, policy_key)
    for policy_key, _ in ENERGY_POLICIES
    for taskset in tasksets_mixed
]

with _mp_ctx.Pool(MAX_WORKERS) as pool:
    mixed_energy_rows = pool.starmap(run_energy_variant, mixed_energy_args)
print(f"Mixed energy benchmark: {len(mixed_energy_rows)} runs complete")

mixed_energy_df = pl.DataFrame(mixed_energy_rows)

# %%
mixed_energy_summary = (
    mixed_energy_df.group_by(["policy", "total_util"])
    .agg(
        mean_avg_power_mw=pl.col("avg_power_mw").mean(),
        acceptance_rate=1.0
        - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum(),
        deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum(),
        completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum(),
    )
    .sort(["policy", "total_util"])
)
mixed_energy_summary

# %%
fig, axes = plt.subplots(1, 4, figsize=(22, 5))

for policy_key, label in policy_labels.items():
    data = mixed_energy_summary.filter(pl.col("policy") == policy_key).sort("total_util")
    axes[0].plot(
        data["total_util"], data["mean_avg_power_mw"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

axes[0].set_xlabel("Total Utilization")
axes[0].set_ylabel("Mean Average Power (mW)")
axes[0].set_title("Average Power Consumption")
axes[0].legend()
axes[0].grid(True, alpha=0.3)

for policy_key, label in policy_labels.items():
    data = mixed_energy_summary.filter(pl.col("policy") == policy_key).sort("total_util")
    axes[1].plot(
        data["total_util"], data["acceptance_rate"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

axes[1].set_xlabel("Total Utilization")
axes[1].set_ylabel("Acceptance Rate")
axes[1].set_title("Acceptance Rate")
axes[1].legend()
axes[1].grid(True, alpha=0.3)

for policy_key, label in policy_labels.items():
    data = mixed_energy_summary.filter(pl.col("policy") == policy_key).sort("total_util")
    axes[2].plot(
        data["total_util"], data["deadline_miss_rate"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

axes[2].set_xlabel("Total Utilization")
axes[2].set_ylabel("Deadline Miss Rate")
axes[2].set_ylim(0, 1)
axes[2].set_title("Deadline Miss Rate")
axes[2].legend()
axes[2].grid(True, alpha=0.3)

for policy_key, label in policy_labels.items():
    data = mixed_energy_summary.filter(pl.col("policy") == policy_key).sort("total_util")
    axes[3].plot(
        data["total_util"], data["completion_rate"],
        marker="o", linewidth=1.5, markersize=4, label=label,
    )

axes[3].set_xlabel("Total Utilization")
axes[3].set_ylabel("Effective Completion Rate")
axes[3].set_ylim(0, 1)
axes[3].set_title("Effective Completion Rate")
axes[3].legend()
axes[3].grid(True, alpha=0.3)

fig.suptitle("Exynos 5422 — Energy Benchmark (mixed-profile tasksets)", fontsize=13)
plt.tight_layout()
plt.show()

# %% [markdown]
# ## Orion O6 — Unbounded allocator comparison
#
# Repeat the unbounded taskset experiment on the Radxa Orion O6
# (CIX P1 CD8180): 4×A720 big + 4×A720 mid + 4×A520 LITTLE (12 cores, 3 clusters).

# %%
PLATFORM_ORION = "platforms/orion6.json"
# Orion O6 capacity: 4×big (perf=1.0) + 4×mid (perf=1.0) + 4×LITTLE (perf=0.444)
ORION_BIG_CAP = 4 * 1.0
ORION_MID_CAP = 4 * 1.0
ORION_LITTLE_CAP = 4 * 0.444
ORION_PLATFORM_CAP = ORION_BIG_CAP + ORION_MID_CAP + ORION_LITTLE_CAP  # 9.776

UTIL_MAX_ORION = 11.0
UTIL_STEP_ORION = 0.1
UTIL_MIN_ORION = 1.0
N_TASKS_ORION = 30  # enough headroom for UUniFast-Discard (avg 11.0/30 = 0.37 << 0.9)
TASKSET_DIR_UNBOUNDED_ORION = "tasksets_unbounded_orion"

util_points_orion = np.round(
    np.arange(UTIL_MIN_ORION, UTIL_MAX_ORION + UTIL_STEP_ORION / 2, UTIL_STEP_ORION), 4
)

# %%
def _generate_one_unbounded_orion(util, scenario_idx):
    seed = 2_000_000_000 + int(round(util * 10000)) * 1100 + scenario_idx
    random.seed(seed)
    utils = uunifast_discard(N_TASKS_ORION, util, UTIL_UNBOUNDED_LO, UTIL_UNBOUNDED_HI)
    folder = os.path.join(TASKSET_DIR_UNBOUNDED_ORION, str(int(round(util * 10))))
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, f"{scenario_idx + 1}.json")
    scenario = sim.from_utilizations(utils, SUCCESS_RATE, COMPRESSION_RATE, seed, NUM_HYPERPERIODS, 0)
    sim.write_scenario(scenario, path)
    return {
        "total_util": round(float(util), 4),
        "seed": seed,
        "utilizations": [float(u) for u in utils],
        "path": path,
    }


orion_gen_args = [(util, idx) for util in util_points_orion for idx in range(N_SCENARIOS_UNBOUNDED)]
with _mp_ctx.Pool(MAX_WORKERS) as pool:
    tasksets_unbounded_orion = pool.starmap(_generate_one_unbounded_orion, orion_gen_args)

print(f"Generated {len(tasksets_unbounded_orion)} Orion O6 unbounded tasksets in {TASKSET_DIR_UNBOUNDED_ORION}/")

# %%
orion_comparison_args = [
    (taskset, is_adaptive, PLATFORM_ORION, mig)
    for _, is_adaptive in [("ff_cap_adaptive_linear", True), ("ff_cap_u_target_1_0", False)]
    for mig in [False, True]
    for taskset in tasksets_unbounded_orion
]

with _mp_ctx.Pool(MAX_WORKERS) as pool:
    orion_comparison_rows = pool.starmap(run_allocator_variant, orion_comparison_args)
print(f"Orion O6 allocator comparison: {len(orion_comparison_rows)} runs complete")

orion_comparison_df = pl.DataFrame(orion_comparison_rows)

# %%
# Per-scenario acceptance rate: accepted / total_arrivals for each (seed, total_util, migration)
orion_comparison_per_scenario = (
    orion_comparison_df.with_columns(
        (pl.col("total_arrivals") - pl.col("rejected_arrivals"))
        .truediv(pl.col("total_arrivals"))
        .alias("acceptance_rate")
    )
)

# Pivot so we have one row per scenario with both allocators side by side (per migration setting)
orion_sig_dfs = {}
for mig in [False, True]:
    mig_label = "migration" if mig else "no_migration"
    sub = orion_comparison_per_scenario.filter(pl.col("migration") == mig)
    adaptive_sc = (
        sub.filter(pl.col("allocator") == "ff_cap_adaptive_linear")
        .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_adaptive")])
    )
    baseline_sc = (
        sub.filter(pl.col("allocator") == "ff_cap_u_target_1_0")
        .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_baseline")])
    )
    paired = (
        adaptive_sc.join(baseline_sc, on=["total_util", "seed"])
        .with_columns((pl.col("acc_adaptive") - pl.col("acc_baseline")).alias("gain"))
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
            "migration": mig,
        })
    orion_sig_dfs[mig_label] = pl.DataFrame(sig_rows)

orion_sig_df = pl.concat(list(orion_sig_dfs.values()))
orion_sig_df

# %%
orion_label_map = {
    "ff_cap_adaptive_linear": "FFCap Adaptive Linear",
    "ff_cap_u_target_1_0": "FFCap (u_target = 1.0)",
}
orion_mig_styles = {False: ("-", "o", ""), True: ("--", "s", " + migration")}

orion_comparison_summary = (
    orion_comparison_df.group_by(["allocator", "migration", "total_util"])
    .agg(
        acceptance_rate=1.0
        - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum(),
    )
    .sort(["allocator", "migration", "total_util"])
)

fig, ax = plt.subplots(figsize=(10, 5))
for name, label in orion_label_map.items():
    for mig, (ls, marker, suffix) in orion_mig_styles.items():
        data = orion_comparison_summary.filter(
            (pl.col("allocator") == name) & (pl.col("migration") == mig)
        ).sort("total_util")
        ax.plot(
            data["total_util"], data["acceptance_rate"],
            marker=marker, linewidth=1.5, linestyle=ls, label=f"{label}{suffix}",
        )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Acceptance Rate")
ax.set_title("Acceptance Rate — Adaptive vs FFCap (u_target = 1.0)\nOrion O6 — Unbounded tasksets")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Deadline misses per allocator — Orion O6

# %%
orion_deadline_summary = (
    orion_comparison_df.group_by(["allocator", "migration", "total_util"])
    .agg(
        deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum(),
    )
    .sort(["allocator", "migration", "total_util"])
)

fig, ax = plt.subplots(figsize=(10, 5))
for name, label in orion_label_map.items():
    for mig, (ls, marker, suffix) in orion_mig_styles.items():
        data = orion_deadline_summary.filter(
            (pl.col("allocator") == name) & (pl.col("migration") == mig)
        ).sort("total_util")
        ax.plot(
            data["total_util"], data["deadline_miss_rate"],
            marker=marker, linewidth=1.5, linestyle=ls, label=f"{label}{suffix}",
        )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Deadline Miss Rate")
ax.set_ylim(0, 1)
ax.set_title("Deadline Miss Rate — Adaptive vs FFCap (u_target = 1.0)\nOrion O6 — Unbounded tasksets")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Effective completion rate — Orion O6

# %%
orion_completion_summary = (
    orion_comparison_df.group_by(["allocator", "migration", "total_util"])
    .agg(
        completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum(),
    )
    .sort(["allocator", "migration", "total_util"])
)

fig, ax = plt.subplots(figsize=(10, 5))
for name, label in orion_label_map.items():
    for mig, (ls, marker, suffix) in orion_mig_styles.items():
        data = orion_completion_summary.filter(
            (pl.col("allocator") == name) & (pl.col("migration") == mig)
        ).sort("total_util")
        ax.plot(
            data["total_util"], data["completion_rate"],
            marker=marker, linewidth=1.5, linestyle=ls, label=f"{label}{suffix}",
        )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Effective Completion Rate")
ax.set_ylim(0, 1)
ax.set_title("Effective Completion Rate — Adaptive vs FFCap (u_target = 1.0)\nOrion O6 — Unbounded tasksets")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Per-cluster utilization — Orion O6

# %%
# Unpack cluster_utils list into individual columns
orion_comparison_expanded = orion_comparison_df.with_columns([
    pl.col("cluster_utils").list.get(0).alias("little_util_orion"),
    pl.col("cluster_utils").list.get(1).alias("big_util_orion"),
    pl.col("cluster_utils").list.get(2).alias("mid_util_orion"),
])

orion_cluster_summary = (
    orion_comparison_expanded.group_by(["allocator", "migration", "total_util"])
    .agg(
        mean_little_util=pl.col("little_util_orion").mean(),
        mean_mid_util=pl.col("mid_util_orion").mean(),
        mean_big_util=pl.col("big_util_orion").mean(),
    )
    .sort(["allocator", "migration", "total_util"])
)

fig, axes = plt.subplots(1, 3, figsize=(18, 5))

cluster_cols = [
    ("mean_little_util", "LITTLE Cluster (A520, perf=0.444)"),
    ("mean_mid_util", "Mid Cluster (A720, perf=1.0)"),
    ("mean_big_util", "Big Cluster (A720, perf=1.0)"),
]

for ax, (col, title) in zip(axes, cluster_cols):
    for name, label in orion_label_map.items():
        for mig, (ls, marker, suffix) in orion_mig_styles.items():
            data = orion_cluster_summary.filter(
                (pl.col("allocator") == name) & (pl.col("migration") == mig)
            ).sort("total_util")
            ax.plot(
                data["total_util"], data[col],
                marker=marker, linewidth=1.5, markersize=4, linestyle=ls,
                label=f"{label}{suffix}",
            )
    ax.set_xlabel("Total Utilization")
    ax.set_ylabel("Mean Admitted Utilization")
    ax.set_title(title)
    ax.legend(fontsize=7)
    ax.grid(True, alpha=0.3)

fig.suptitle("Orion O6 — Per-Cluster Admitted Utilization", fontsize=13)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Acceptance rate gain (Adaptive − FFCap) — Orion O6

# %%
fig, axes = plt.subplots(1, 2, figsize=(20, 5), sharey=True)

for ax, (mig_key, mig_label) in zip(axes, [("no_migration", "No migration"), ("migration", "With migration")]):
    sub = orion_sig_dfs[mig_key]
    utils_arr = sub["total_util"].to_numpy()
    gains_arr = sub["mean_gain_pct"].to_numpy()
    sig_labels_arr = sub["significance"].to_list()

    colors = [SIG_COLORS[s] for s in sig_labels_arr]
    bars = ax.bar(
        [f"{u:.1f}" for u in utils_arr], gains_arr,
        color=colors, edgecolor="black", linewidth=0.5,
    )

    for bar, label in zip(bars, sig_labels_arr):
        y = bar.get_height()
        va = "bottom" if y >= 0 else "top"
        offset = 0.15 if y >= 0 else -0.15
        ax.text(bar.get_x() + bar.get_width() / 2, y + offset, label,
                ha="center", va=va, fontsize=5, fontweight="bold")

    ax.axhline(0, color="black", linewidth=0.8)
    ax.set_xlabel("Total Utilization")
    ax.set_title(f"Acceptance Rate Gain (Adaptive − FFCap)\n{mig_label}")
    ax.tick_params(axis="x", rotation=45, labelsize=6)
    ax.grid(True, axis="y", alpha=0.3)

axes[0].set_ylabel("Mean Acceptance Rate Gain (%)")

orion_legend_elements = [
    Patch(facecolor="darkgreen", edgecolor="black", label="p < 0.001 (***)"),
    Patch(facecolor="seagreen", edgecolor="black", label="p < 0.01 (**)"),
    Patch(facecolor="goldenrod", edgecolor="black", label="p < 0.05 (*)"),
    Patch(facecolor="lightgray", edgecolor="black", label="not significant (ns)"),
]
axes[1].legend(handles=orion_legend_elements, fontsize=8, loc="upper left")
fig.suptitle("Orion O6 — Acceptance Rate Gain (Adaptive − FFCap)", fontsize=13)
plt.tight_layout()
plt.show()

# %% [markdown]
# ### Cluster migration count — Orion O6
#
# Average number of inter-cluster migrations per scenario when migration
# is enabled, broken down by allocator.

# %%
orion_migration_summary = (
    orion_comparison_df.filter(pl.col("migration") == True)
    .group_by(["allocator", "total_util"])
    .agg(mean_migrations=pl.col("cluster_migrations").mean())
    .sort(["allocator", "total_util"])
)

fig, ax = plt.subplots(figsize=(10, 5))
for name, label in orion_label_map.items():
    data = orion_migration_summary.filter(pl.col("allocator") == name).sort("total_util")
    ax.plot(
        data["total_util"], data["mean_migrations"],
        marker="o", linewidth=1.5, label=label,
    )

ax.set_xlabel("Total Utilization")
ax.set_ylabel("Mean Cluster Migrations")
ax.set_title("Inter-Cluster Migrations — Orion O6 (migration enabled)")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

# %% [markdown]
# ## Orion O6 — Energy consumption
#
# Compare energy consumption of GRUB, GRUB-PA, CSF, and FFA
# policies on Orion O6 unbounded tasksets, with and without migration.

# %%
orion_energy_args = [
    (taskset, policy_key, PLATFORM_ORION, mig)
    for policy_key, _ in ENERGY_POLICIES
    for mig in [False, True]
    for taskset in tasksets_unbounded_orion
]

with _mp_ctx.Pool(MAX_WORKERS) as pool:
    orion_energy_rows = pool.starmap(run_energy_variant, orion_energy_args)
print(f"Orion O6 energy benchmark: {len(orion_energy_rows)} runs complete")

orion_energy_df = pl.DataFrame(orion_energy_rows)

# %%
orion_energy_summary = (
    orion_energy_df.group_by(["policy", "migration", "total_util"])
    .agg(
        mean_avg_power_mw=pl.col("avg_power_mw").mean(),
        acceptance_rate=1.0
        - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum(),
        deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum(),
        completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum(),
    )
    .sort(["policy", "migration", "total_util"])
)
orion_energy_summary

# %%
orion_energy_mig_styles = {False: ("-", "o", ""), True: ("--", "s", " + migration")}

fig, axes = plt.subplots(1, 4, figsize=(22, 5))

# Average power consumption
for policy_key, label in policy_labels.items():
    for mig, (ls, marker, suffix) in orion_energy_mig_styles.items():
        data = orion_energy_summary.filter(
            (pl.col("policy") == policy_key) & (pl.col("migration") == mig)
        ).sort("total_util")
        axes[0].plot(
            data["total_util"], data["mean_avg_power_mw"],
            marker=marker, linewidth=1.5, markersize=4, linestyle=ls,
            label=f"{label}{suffix}",
        )

axes[0].set_xlabel("Total Utilization")
axes[0].set_ylabel("Mean Average Power (mW)")
axes[0].set_title("Average Power Consumption")
axes[0].legend(fontsize=7)
axes[0].grid(True, alpha=0.3)

# Acceptance rate
for policy_key, label in policy_labels.items():
    for mig, (ls, marker, suffix) in orion_energy_mig_styles.items():
        data = orion_energy_summary.filter(
            (pl.col("policy") == policy_key) & (pl.col("migration") == mig)
        ).sort("total_util")
        axes[1].plot(
            data["total_util"], data["acceptance_rate"],
            marker=marker, linewidth=1.5, markersize=4, linestyle=ls,
            label=f"{label}{suffix}",
        )

axes[1].set_xlabel("Total Utilization")
axes[1].set_ylabel("Acceptance Rate")
axes[1].set_title("Acceptance Rate")
axes[1].legend(fontsize=7)
axes[1].grid(True, alpha=0.3)

# Deadline misses
for policy_key, label in policy_labels.items():
    for mig, (ls, marker, suffix) in orion_energy_mig_styles.items():
        data = orion_energy_summary.filter(
            (pl.col("policy") == policy_key) & (pl.col("migration") == mig)
        ).sort("total_util")
        axes[2].plot(
            data["total_util"], data["deadline_miss_rate"],
            marker=marker, linewidth=1.5, markersize=4, linestyle=ls,
            label=f"{label}{suffix}",
        )

axes[2].set_xlabel("Total Utilization")
axes[2].set_ylabel("Deadline Miss Rate")
axes[2].set_ylim(0, 1)
axes[2].set_title("Deadline Miss Rate")
axes[2].legend(fontsize=7)
axes[2].grid(True, alpha=0.3)

# Effective completion rate
for policy_key, label in policy_labels.items():
    for mig, (ls, marker, suffix) in orion_energy_mig_styles.items():
        data = orion_energy_summary.filter(
            (pl.col("policy") == policy_key) & (pl.col("migration") == mig)
        ).sort("total_util")
        axes[3].plot(
            data["total_util"], data["completion_rate"],
            marker=marker, linewidth=1.5, markersize=4, linestyle=ls,
            label=f"{label}{suffix}",
        )

axes[3].set_xlabel("Total Utilization")
axes[3].set_ylabel("Effective Completion Rate")
axes[3].set_ylim(0, 1)
axes[3].set_title("Effective Completion Rate")
axes[3].legend(fontsize=7)
axes[3].grid(True, alpha=0.3)

fig.suptitle("Orion O6 — Energy Benchmark (unbounded tasksets, migration comparison)", fontsize=13)
plt.tight_layout()
plt.show()

# %% [markdown]
# ## Export plot data to CSV for pgfplots

# %%
OUTDIR = os.path.expanduser("~/Nextcloud/these/manuscript/chap5-dynamic-allocation/data")
os.makedirs(OUTDIR, exist_ok=True)

# Unbounded: acceptance rate per allocator
comparison_summary.write_csv(os.path.join(OUTDIR, "ub_unbounded_acceptance.csv"))
print(f"ub_unbounded_acceptance.csv: {comparison_summary.shape[0]} rows")

# Unbounded: deadline miss rate per allocator
deadline_summary.write_csv(os.path.join(OUTDIR, "ub_unbounded_deadline_miss.csv"))
print(f"ub_unbounded_deadline_miss.csv: {deadline_summary.shape[0]} rows")

# Unbounded: per-cluster utilization per allocator
cluster_summary.write_csv(os.path.join(OUTDIR, "ub_unbounded_cluster_util.csv"))
print(f"ub_unbounded_cluster_util.csv: {cluster_summary.shape[0]} rows")

# Unbounded: acceptance rate gain (adaptive - baseline) with significance
sig_df.write_csv(os.path.join(OUTDIR, "ub_unbounded_sig.csv"))
print(f"ub_unbounded_sig.csv: {sig_df.shape[0]} rows")

# Unbounded: effective completion rate per allocator
completion_summary.write_csv(os.path.join(OUTDIR, "ub_unbounded_completion.csv"))
print(f"ub_unbounded_completion.csv: {completion_summary.shape[0]} rows")

# Unbounded: energy benchmark (power, acceptance, deadline miss, completion per policy)
energy_summary.write_csv(os.path.join(OUTDIR, "ub_energy_summary.csv"))
print(f"ub_energy_summary.csv: {energy_summary.shape[0]} rows")

# Mixed: acceptance rate per allocator
mixed_comparison_summary.write_csv(os.path.join(OUTDIR, "ub_mixed_acceptance.csv"))
print(f"ub_mixed_acceptance.csv: {mixed_comparison_summary.shape[0]} rows")

# Mixed: acceptance rate gain with significance
mixed_sig_df.write_csv(os.path.join(OUTDIR, "ub_mixed_sig.csv"))
print(f"ub_mixed_sig.csv: {mixed_sig_df.shape[0]} rows")

# Mixed: effective completion rate per allocator
mixed_completion_summary.write_csv(os.path.join(OUTDIR, "ub_mixed_completion.csv"))
print(f"ub_mixed_completion.csv: {mixed_completion_summary.shape[0]} rows")

# Mixed: energy benchmark (power, acceptance, deadline miss, completion per policy)
mixed_energy_summary.write_csv(os.path.join(OUTDIR, "ub_mixed_energy_summary.csv"))
print(f"ub_mixed_energy_summary.csv: {mixed_energy_summary.shape[0]} rows")

# Orion O6: acceptance rate per allocator
orion_comparison_summary.write_csv(os.path.join(OUTDIR, "ub_orion_acceptance.csv"))
print(f"ub_orion_acceptance.csv: {orion_comparison_summary.shape[0]} rows")

# Orion O6: deadline miss rate per allocator
orion_deadline_summary.write_csv(os.path.join(OUTDIR, "ub_orion_deadline_miss.csv"))
print(f"ub_orion_deadline_miss.csv: {orion_deadline_summary.shape[0]} rows")

# Orion O6: per-cluster utilization per allocator
orion_cluster_summary.write_csv(os.path.join(OUTDIR, "ub_orion_cluster_util.csv"))
print(f"ub_orion_cluster_util.csv: {orion_cluster_summary.shape[0]} rows")

# Orion O6: acceptance rate gain with significance
orion_sig_df.write_csv(os.path.join(OUTDIR, "ub_orion_sig.csv"))
print(f"ub_orion_sig.csv: {orion_sig_df.shape[0]} rows")

# Orion O6: effective completion rate per allocator
orion_completion_summary.write_csv(os.path.join(OUTDIR, "ub_orion_completion.csv"))
print(f"ub_orion_completion.csv: {orion_completion_summary.shape[0]} rows")

# Orion O6: energy benchmark
orion_energy_summary.write_csv(os.path.join(OUTDIR, "ub_orion_energy_summary.csv"))
print(f"ub_orion_energy_summary.csv: {orion_energy_summary.shape[0]} rows")

# Orion O6: migration count
orion_migration_summary.write_csv(os.path.join(OUTDIR, "ub_orion_migration_count.csv"))
print(f"ub_orion_migration_count.csv: {orion_migration_summary.shape[0]} rows")

print(f"\nExported 17 CSV files to {OUTDIR}/")

