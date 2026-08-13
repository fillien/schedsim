#!/usr/bin/env python3
"""Standalone Exynos 5422 benchmark script.

Runs unbounded + mixed-profile allocator comparisons and energy benchmarks,
then exports all CSVs. No notebook timeout constraints.
"""
import os
import sys
import random
import time
import multiprocessing as mp
from pathlib import Path

sys.path.insert(0, str(Path("build/python").resolve()))

import pyschedsim as sim
import polars as pl
import numpy as np
from scipy import stats

MAX_WORKERS = max(1, (os.cpu_count() or 1) - 1)
_mp_ctx = mp.get_context("fork")
print(f"Using {MAX_WORKERS} worker processes")

# ---------------------------------------------------------------------------
# Shared constants
# ---------------------------------------------------------------------------
PLATFORM = "platforms/exynos5422.json"
SUCCESS_RATE = 0.5
COMPRESSION_RATE = 0.5
NUM_HYPERPERIODS = 100
MIN_JOBS = 700
UTIL_PER_TASK_MIN = 0.001
UTIL_MAX = 5.5

ENERGY_POLICIES = [("grub", "GRUB"), ("grub_pa", "GRUB-PA"), ("csf", "CSF"), ("ffa", "FFA")]
OUTDIR = os.path.expanduser("~/Nextcloud/these/manuscript/chap5-dynamic-allocation/data/exynos5422-unbounded-mixed")

# Unbounded constants
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

# Mixed constants
TASKSET_DIR_MIXED = "tasksets_mixed"
N_SCENARIOS_MIXED = 1000
UTIL_MIN_MIXED = 1.0
UTIL_MAX_MIXED = 5.5
UTIL_STEP_MIXED = 0.1

UTIL_CLASSES = [
    (0.40, 0.01, 0.12),  # light
    (0.20, 0.25, 0.33),  # medium
    (0.40, 0.50, 0.80),  # heavy
]
CLASS_MIDS = [(lo + hi) / 2 for _, lo, hi in UTIL_CLASSES]
FRAC_MID_SUM = sum(f * m for (f, _, _), m in zip(UTIL_CLASSES, CLASS_MIDS))

util_points_mixed = np.round(
    np.arange(UTIL_MIN_MIXED, UTIL_MAX_MIXED + UTIL_STEP_MIXED / 2, UTIL_STEP_MIXED), 4
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def uunifast_discard(n, u_total, umin=0.0, umax=1.0, max_attempts=1_000_000):
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


def sig_label(p: float) -> str:
    if p < 0.001:
        return "***"
    if p < 0.01:
        return "**"
    if p < 0.05:
        return "*"
    return "ns"


def compute_class_counts(total_util: float) -> list[int]:
    counts = [max(1, round(total_util * f / FRAC_MID_SUM)) for f, _, _ in UTIL_CLASSES]
    while sum(nc * hi for nc, (_, _, hi) in zip(counts, UTIL_CLASSES)) < total_util:
        counts[-1] += 1
    return counts


def generate_mixed_utilizations(total_util: float, seed: int) -> list[float]:
    random.seed(seed)
    cc = compute_class_counts(total_util)
    weights = [nc * mid for nc, mid in zip(cc, CLASS_MIDS)]
    total_weight = sum(weights)
    class_utils = [total_util * w / total_weight for w in weights]

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


def run_allocator_variant(taskset: dict, adaptive: bool, platform_path: str = PLATFORM,
                          migration: bool = False) -> dict:
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
        "cluster_utils": [c.utilization for c in sorted_clusters],
        "little_util": sorted_clusters[0].utilization if len(sorted_clusters) > 0 else 0.0,
        "big_util": sorted_clusters[-1].utilization if len(sorted_clusters) > 1 else 0.0,
    }


def run_energy_variant(taskset: dict, dvfs_policy: str, platform_path: str = PLATFORM,
                       migration: bool = False) -> dict:
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
            pass
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


def compute_sig_test(comparison_df):
    """Compute Wilcoxon signed-rank test for adaptive vs baseline."""
    per_scenario = comparison_df.with_columns(
        (pl.col("total_arrivals") - pl.col("rejected_arrivals"))
        .truediv(pl.col("total_arrivals"))
        .alias("acceptance_rate")
    )
    adaptive_sc = (
        per_scenario.filter(pl.col("allocator") == "ff_cap_adaptive_linear")
        .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_adaptive")])
    )
    baseline_sc = (
        per_scenario.filter(pl.col("allocator") == "ff_cap_u_target_1_0")
        .select(["total_util", "seed", pl.col("acceptance_rate").alias("acc_baseline")])
    )
    paired = (
        adaptive_sc.join(baseline_sc, on=["total_util", "seed"])
        .with_columns((pl.col("acc_adaptive") - pl.col("acc_baseline")).alias("gain"))
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
    return pl.DataFrame(sig_rows)


# ---------------------------------------------------------------------------
# Taskset generators
# ---------------------------------------------------------------------------
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


# ===================================================================
if __name__ == "__main__":
    t0 = time.time()
    os.makedirs(OUTDIR, exist_ok=True)

    # ===========================================================
    # PART 1: Unbounded tasksets
    # ===========================================================
    print("=" * 60)
    print("PART 1: Unbounded tasksets on Exynos 5422")
    print("=" * 60)

    # -- Generate --
    print(f"Generating unbounded tasksets ({len(util_points_unbounded)} × {N_SCENARIOS_UNBOUNDED}) ...")
    gen_args = [(util, idx) for util in util_points_unbounded for idx in range(N_SCENARIOS_UNBOUNDED)]
    with _mp_ctx.Pool(MAX_WORKERS) as pool:
        tasksets_unbounded = pool.starmap(_generate_one_unbounded, gen_args)
    print(f"  {len(tasksets_unbounded)} tasksets in {time.time() - t0:.0f}s")

    # -- Allocator comparison --
    t1 = time.time()
    print("Running unbounded allocator comparison ...")
    comparison_args = [
        (taskset, is_adaptive)
        for _, is_adaptive in [("adaptive", True), ("baseline", False)]
        for taskset in tasksets_unbounded
    ]
    with _mp_ctx.Pool(MAX_WORKERS) as pool:
        comparison_rows = pool.starmap(run_allocator_variant, comparison_args)
    print(f"  {len(comparison_rows)} runs in {time.time() - t1:.0f}s")
    comparison_df = pl.DataFrame(comparison_rows)

    # Summaries
    comparison_summary = (
        comparison_df.group_by(["allocator", "total_util"])
        .agg(acceptance_rate=1.0 - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum())
        .sort(["allocator", "total_util"])
    )
    deadline_summary = (
        comparison_df.group_by(["allocator", "total_util"])
        .agg(deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum())
        .sort(["allocator", "total_util"])
    )
    completion_summary = (
        comparison_df.group_by(["allocator", "total_util"])
        .agg(completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum())
        .sort(["allocator", "total_util"])
    )
    cluster_summary = (
        comparison_df.group_by(["allocator", "total_util"])
        .agg(
            mean_little_util=pl.col("little_util").mean(),
            mean_big_util=pl.col("big_util").mean(),
        )
        .sort(["allocator", "total_util"])
    )
    sig_df = compute_sig_test(comparison_df)

    # -- Energy benchmark --
    t2 = time.time()
    print("Running unbounded energy benchmark ...")
    energy_args = [
        (taskset, policy_key)
        for policy_key, _ in ENERGY_POLICIES
        for taskset in tasksets_unbounded
    ]
    with _mp_ctx.Pool(MAX_WORKERS) as pool:
        energy_rows = pool.starmap(run_energy_variant, energy_args)
    print(f"  {len(energy_rows)} runs in {time.time() - t2:.0f}s")
    energy_df = pl.DataFrame(energy_rows)

    energy_summary = (
        energy_df.group_by(["policy", "total_util"])
        .agg(
            mean_avg_power_mw=pl.col("avg_power_mw").mean(),
            acceptance_rate=1.0 - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum(),
            deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum(),
            completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum(),
        )
        .sort(["policy", "total_util"])
    )

    # -- Export unbounded CSVs (PGFplots wide format) --
    from pgfplots_export import export_alloc_csvs, export_energy_csvs

    unbounded_dir = os.path.join(OUTDIR, "unbounded")
    print("Exporting unbounded allocator CSVs:")
    # Join summaries for single-pass export
    alloc_joined = (
        comparison_summary
        .join(deadline_summary, on=["allocator", "total_util"])
        .join(completion_summary, on=["allocator", "total_util"])
    )
    export_alloc_csvs(alloc_joined, cluster_summary, sig_df, unbounded_dir)

    print("Exporting unbounded energy CSVs:")
    export_energy_csvs(energy_summary, unbounded_dir)

    # ===========================================================
    # PART 2: Mixed-profile tasksets
    # ===========================================================
    print()
    print("=" * 60)
    print("PART 2: Mixed-profile tasksets on Exynos 5422")
    print("=" * 60)

    # -- Generate --
    t3 = time.time()
    print(f"Generating mixed tasksets ({len(util_points_mixed)} × {N_SCENARIOS_MIXED}) ...")
    mixed_gen_args = [(util, idx) for util in util_points_mixed for idx in range(N_SCENARIOS_MIXED)]
    with _mp_ctx.Pool(MAX_WORKERS) as pool:
        tasksets_mixed = pool.starmap(_generate_one_mixed, mixed_gen_args)
    print(f"  {len(tasksets_mixed)} tasksets in {time.time() - t3:.0f}s")

    # -- Allocator comparison --
    t4 = time.time()
    print("Running mixed allocator comparison ...")
    mixed_comparison_args = [
        (taskset, is_adaptive)
        for _, is_adaptive in [("adaptive", True), ("baseline", False)]
        for taskset in tasksets_mixed
    ]
    with _mp_ctx.Pool(MAX_WORKERS) as pool:
        mixed_comparison_rows = pool.starmap(run_allocator_variant, mixed_comparison_args)
    print(f"  {len(mixed_comparison_rows)} runs in {time.time() - t4:.0f}s")
    mixed_comparison_df = pl.DataFrame(mixed_comparison_rows)

    # Summaries
    mixed_comparison_summary = (
        mixed_comparison_df.group_by(["allocator", "total_util"])
        .agg(acceptance_rate=1.0 - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum())
        .sort(["allocator", "total_util"])
    )
    mixed_deadline_summary = (
        mixed_comparison_df.group_by(["allocator", "total_util"])
        .agg(deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum())
        .sort(["allocator", "total_util"])
    )
    mixed_completion_summary = (
        mixed_comparison_df.group_by(["allocator", "total_util"])
        .agg(completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum())
        .sort(["allocator", "total_util"])
    )
    mixed_sig_df = compute_sig_test(mixed_comparison_df)

    # -- Energy benchmark --
    t5 = time.time()
    print("Running mixed energy benchmark ...")
    mixed_energy_args = [
        (taskset, policy_key)
        for policy_key, _ in ENERGY_POLICIES
        for taskset in tasksets_mixed
    ]
    with _mp_ctx.Pool(MAX_WORKERS) as pool:
        mixed_energy_rows = pool.starmap(run_energy_variant, mixed_energy_args)
    print(f"  {len(mixed_energy_rows)} runs in {time.time() - t5:.0f}s")
    mixed_energy_df = pl.DataFrame(mixed_energy_rows)

    mixed_energy_summary = (
        mixed_energy_df.group_by(["policy", "total_util"])
        .agg(
            mean_avg_power_mw=pl.col("avg_power_mw").mean(),
            acceptance_rate=1.0 - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum(),
            deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum(),
            completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum(),
        )
        .sort(["policy", "total_util"])
    )

    # -- Export mixed CSVs (PGFplots wide format) --
    mixed_dir = os.path.join(OUTDIR, "mixed")
    print("Exporting mixed allocator CSVs:")
    mixed_alloc_joined = (
        mixed_comparison_summary
        .join(mixed_deadline_summary, on=["allocator", "total_util"])
        .join(mixed_completion_summary, on=["allocator", "total_util"])
    )
    export_alloc_csvs(mixed_alloc_joined, None, mixed_sig_df, mixed_dir)

    print("Exporting mixed energy CSVs:")
    export_energy_csvs(mixed_energy_summary, mixed_dir)

    # ===========================================================
    elapsed = time.time() - t0
    print(f"\nDone in {elapsed / 60:.1f} minutes. CSVs in {OUTDIR}/")
