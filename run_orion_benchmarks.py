#!/usr/bin/env python3
"""Standalone Orion O6 benchmark script.

Runs the allocator comparison and energy benchmark for the Orion O6
platform and exports all CSVs. No notebook timeout constraints.
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
PLATFORM_ORION = "platforms/orion6.json"
SUCCESS_RATE = 0.5
COMPRESSION_RATE = 0.5
NUM_HYPERPERIODS = 100
UTIL_PER_TASK_MIN = 0.001
UTIL_UNBOUNDED_LO = UTIL_PER_TASK_MIN
UTIL_UNBOUNDED_HI = 0.9

UTIL_MAX_ORION = 11.0
UTIL_STEP_ORION = 0.1
UTIL_MIN_ORION = 1.0
N_TASKS_ORION = 30
N_SCENARIOS = 1000
TASKSET_DIR = "tasksets_unbounded_orion"
ENERGY_POLICIES = [("grub", "GRUB"), ("grub_pa", "GRUB-PA"), ("csf", "CSF"), ("ffa", "FFA")]
OUTDIR = os.path.expanduser("~/Nextcloud/these/manuscript/chap5-dynamic-allocation/data/orion-o6-unbounded")

util_points = np.round(
    np.arange(UTIL_MIN_ORION, UTIL_MAX_ORION + UTIL_STEP_ORION / 2, UTIL_STEP_ORION), 4
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


def run_allocator_variant(taskset: dict, adaptive: bool, platform_path: str = PLATFORM_ORION,
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


def run_energy_variant(taskset: dict, dvfs_policy: str, platform_path: str = PLATFORM_ORION,
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


# ---------------------------------------------------------------------------
# 1. Generate tasksets (reuse if already on disk)
# ---------------------------------------------------------------------------
def _generate_one(util, scenario_idx):
    seed = 2_000_000_000 + int(round(util * 10000)) * 1100 + scenario_idx
    random.seed(seed)
    utils = uunifast_discard(N_TASKS_ORION, util, UTIL_UNBOUNDED_LO, UTIL_UNBOUNDED_HI)
    folder = os.path.join(TASKSET_DIR, str(int(round(util * 10))))
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


if __name__ == "__main__":
    t0 = time.time()

    # -- Generate tasksets --
    print(f"Generating tasksets ({len(util_points)} util points × {N_SCENARIOS} scenarios) ...")
    gen_args = [(util, idx) for util in util_points for idx in range(N_SCENARIOS)]
    with _mp_ctx.Pool(MAX_WORKERS) as pool:
        tasksets = pool.starmap(_generate_one, gen_args)
    print(f"  {len(tasksets)} tasksets in {time.time() - t0:.0f}s")

    # -- Allocator comparison --
    t1 = time.time()
    print("Running allocator comparison (4 variants × all tasksets) ...")
    comparison_args = [
        (taskset, is_adaptive, PLATFORM_ORION, mig)
        for _, is_adaptive in [("adaptive", True), ("baseline", False)]
        for mig in [False, True]
        for taskset in tasksets
    ]
    with _mp_ctx.Pool(MAX_WORKERS) as pool:
        comparison_rows = pool.starmap(run_allocator_variant, comparison_args)
    print(f"  {len(comparison_rows)} runs in {time.time() - t1:.0f}s")
    comparison_df = pl.DataFrame(comparison_rows)

    # -- Allocator summaries --
    orion_comparison_summary = (
        comparison_df.group_by(["allocator", "migration", "total_util"])
        .agg(
            acceptance_rate=1.0
            - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum(),
        )
        .sort(["allocator", "migration", "total_util"])
    )

    orion_deadline_summary = (
        comparison_df.group_by(["allocator", "migration", "total_util"])
        .agg(
            deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum(),
        )
        .sort(["allocator", "migration", "total_util"])
    )

    orion_completion_summary = (
        comparison_df.group_by(["allocator", "migration", "total_util"])
        .agg(
            completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum(),
        )
        .sort(["allocator", "migration", "total_util"])
    )

    # Per-cluster utilization
    orion_comparison_expanded = comparison_df.with_columns([
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

    # Migration count
    orion_migration_summary = (
        comparison_df.filter(pl.col("migration") == True)
        .group_by(["allocator", "total_util"])
        .agg(mean_migrations=pl.col("cluster_migrations").mean())
        .sort(["allocator", "total_util"])
    )

    # Significance test (per migration setting)
    comparison_per_scenario = comparison_df.with_columns(
        (pl.col("total_arrivals") - pl.col("rejected_arrivals"))
        .truediv(pl.col("total_arrivals"))
        .alias("acceptance_rate")
    )
    sig_parts = []
    for mig in [False, True]:
        sub = comparison_per_scenario.filter(pl.col("migration") == mig)
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
        )
        for util in sorted(paired["total_util"].unique().to_list()):
            gains = paired.filter(pl.col("total_util") == util)["gain"].to_numpy()
            mean_gain = float(gains.mean())
            if np.all(gains == 0):
                p_val = 1.0
            else:
                _, p_val = stats.wilcoxon(gains, alternative="two-sided")
            sig_parts.append({
                "total_util": util,
                "mean_gain_pct": round(mean_gain * 100, 4),
                "p_value": float(p_val),
                "significance": sig_label(p_val),
                "migration": mig,
            })
    orion_sig_df = pl.DataFrame(sig_parts)

    # -- Energy benchmark --
    t2 = time.time()
    print("Running energy benchmark (8 variants × all tasksets) ...")
    energy_args = [
        (taskset, policy_key, PLATFORM_ORION, mig)
        for policy_key, _ in ENERGY_POLICIES
        for mig in [False, True]
        for taskset in tasksets
    ]
    with _mp_ctx.Pool(MAX_WORKERS) as pool:
        energy_rows = pool.starmap(run_energy_variant, energy_args)
    print(f"  {len(energy_rows)} runs in {time.time() - t2:.0f}s")
    orion_energy_df = pl.DataFrame(energy_rows)

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

    # -- Export PGFplots-ready CSVs (wide format) --
    from pgfplots_export import export_alloc_csvs, export_energy_csvs

    print("Exporting allocator CSVs (wide format):")
    export_alloc_csvs(
        orion_comparison_summary.join(orion_deadline_summary, on=["allocator", "migration", "total_util"])
            .join(orion_completion_summary, on=["allocator", "migration", "total_util"]),
        orion_cluster_summary,
        orion_sig_df,
        OUTDIR,
        has_migration=True,
        migration_summary=orion_migration_summary,
    )

    print("Exporting energy CSVs (wide format):")
    export_energy_csvs(orion_energy_summary, OUTDIR, has_migration=True)

    elapsed = time.time() - t0
    print(f"\nDone in {elapsed / 60:.1f} minutes. CSVs in {OUTDIR}/")
