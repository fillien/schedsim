#!/usr/bin/env python3
"""Sweep ff_cap u_target values and report the best one for a taskset bag.

Uses pyschedsim for in-process simulation (no subprocess overhead).
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path
from typing import Dict, Optional, Sequence

import pyschedsim


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Sweep ff_cap targets and report the best one for a taskset bag.",
    )
    parser.add_argument(
        "--total-util",
        required=True,
        help="Total utilization corresponding to a min_tasksets/<util>/ folder (e.g., 4.7 or 47).",
    )
    parser.add_argument(
        "--lower-bound",
        type=float,
        default=0.225,
        help="Lower bound (inclusive) for the search interval. Default: 0.225",
    )
    parser.add_argument(
        "--upper-bound",
        type=float,
        default=0.4,
        help="Upper bound (inclusive) for the search interval. Default: 0.4",
    )
    parser.add_argument(
        "--initial-step",
        type=float,
        default=0.005,
        help="Step size for the coarse sweep. Default: 0.005",
    )
    parser.add_argument(
        "--min-step",
        type=float,
        default=0.00025,
        help="Stop refining once the step falls below this value. Default: 0.00025",
    )
    parser.add_argument(
        "--refine-factor",
        type=float,
        default=4.0,
        help="Factor by which the step is divided after each refinement. Default: 4.0",
    )
    parser.add_argument(
        "--max-iters",
        type=int,
        default=8,
        help="Maximum refinement rounds. Default: 8",
    )
    parser.add_argument(
        "--platform",
        default="platforms/exynos5422.json",
        help="Platform description JSON file. Default: platforms/exynos5422.json",
    )
    parser.add_argument(
        "--scheduler",
        default="grub",
        help="Scheduler name: grub or cash. Default: grub",
    )
    parser.add_argument(
        "--min-tasksets",
        default="min_tasksets",
        help="Base directory containing utilization folders. Default: min_tasksets",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print per-run details.",
    )
    return parser.parse_args()


def resolve_repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def sanitize_for_filename(value: str) -> str:
    cleaned = ''.join(ch if ch.isalnum() else '_' for ch in value.strip())
    cleaned = cleaned.strip('_')
    return cleaned or "util"


def locate_taskset_folder(base: Path, util_arg: str) -> Path:
    util_arg = util_arg.strip()
    direct = base / util_arg
    if direct.is_dir():
        return direct

    try:
        util_float = float(util_arg)
    except ValueError as exc:
        raise FileNotFoundError(
            f"Could not interpret total utilization '{util_arg}'."
        ) from exc

    candidates = {
        base / f"{int(round(util_float))}",
        base / f"{int(round(util_float * 10))}",
        base / f"{int(round(util_float * 100))}",
    }
    for candidate in candidates:
        if candidate.is_dir():
            return candidate

    available = ", ".join(sorted(p.name for p in base.iterdir() if p.is_dir()))
    raise FileNotFoundError(
        f"No taskset folder found for total utilization '{util_arg}'. "
        f"Available folders: {available}"
    )


def run_simulation(
    platform_json: str,
    scenario_data: pyschedsim.ScenarioData,
    scheduler: str,
    target: Optional[float] = None,
) -> tuple[int, int]:
    """Run a single ff_cap simulation and return (rejected_arrivals, total_arrivals).

    Total arrivals = total_jobs (placed) + rejected_arrivals.
    """
    engine = pyschedsim.Engine()
    pyschedsim.load_platform_from_string(engine, platform_json)
    tasks = pyschedsim.inject_scenario(engine, scenario_data)
    for i, task in enumerate(tasks):
        pyschedsim.schedule_arrivals(engine, task, scenario_data.tasks[i].jobs)
    engine.platform.finalize()

    # Build per-cluster schedulers and clusters
    platform = engine.platform
    ref_freq_max = max(
        platform.clock_domain(i).freq_max
        for i in range(platform.clock_domain_count)
    )

    schedulers = []
    clusters = []
    for i in range(platform.clock_domain_count):
        cd = platform.clock_domain(i)
        procs = cd.get_processors()
        if not procs:
            continue
        sched = pyschedsim.EdfScheduler(engine, procs)
        if scheduler == "grub":
            sched.enable_grub()
        elif scheduler == "cash":
            sched.enable_cash()
        perf = procs[0].type().performance
        cluster = pyschedsim.Cluster(cd, sched, perf, ref_freq_max)
        if target is not None and perf < 1.0:
            cluster.set_u_target(target)
        schedulers.append(sched)
        clusters.append(cluster)

    alloc = pyschedsim.FFCapAllocator(engine, clusters)

    trace_writer = pyschedsim.MemoryTraceWriter()
    engine.set_trace_writer(trace_writer)
    engine.run()

    metrics = trace_writer.compute_metrics()
    rejected = metrics.rejected_arrivals
    total = metrics.total_jobs + rejected
    return rejected, total


def evaluate_target(
    target: float,
    scenarios: Sequence[pyschedsim.ScenarioData],
    scenario_count: int,
    scenario_files: Sequence[Path],
    platform_json: str,
    scheduler: str,
    verbose: bool,
) -> dict:
    cache: Dict[str, dict] = evaluate_target.cache  # type: ignore[attr-defined]

    target_formatted = float(f"{target:.6f}")
    key = f"{target_formatted:.6f}"
    if key in cache:
        return cache[key]

    per_scenario = []
    total_rejects = 0
    total_arrivals = 0
    sum_share = 0.0
    for i, scenario in enumerate(scenarios):
        rejects, arrivals = run_simulation(
            platform_json, scenario, scheduler, target=target_formatted,
        )
        share = rejects / arrivals if arrivals else 0.0
        per_scenario.append((scenario_files[i], rejects, arrivals, share))
        total_rejects += rejects
        total_arrivals += arrivals
        sum_share += share

    mean_share = sum_share / scenario_count if scenario_count else 0.0
    entry = {
        "target": target_formatted,
        "total_rejects": total_rejects,
        "total_arrivals": total_arrivals,
        "mean_share": mean_share,
        "per_scenario": per_scenario,
    }
    cache[key] = entry
    return entry


evaluate_target.cache = {}  # type: ignore[attr-defined]


def format_scenario(path: Path, repo_root: Path) -> str:
    try:
        return str(path.relative_to(repo_root))
    except ValueError:
        return str(path)


def main() -> int:
    args = parse_args()
    repo_root = resolve_repo_root()

    platform_path = (repo_root / args.platform).resolve()
    if not platform_path.exists():
        raise SystemExit(f"Platform file not found at {platform_path}.")

    min_tasksets_base = (repo_root / args.min_tasksets).resolve()
    if not min_tasksets_base.is_dir():
        raise SystemExit(f"Taskset base directory not found at {min_tasksets_base}.")

    try:
        target_folder = locate_taskset_folder(min_tasksets_base, args.total_util)
    except FileNotFoundError as exc:
        raise SystemExit(str(exc))

    scenario_files = sorted(path for path in target_folder.glob("*.json"))
    if not scenario_files:
        raise SystemExit(f"No taskset JSON files found in {target_folder}.")
    scenario_count = len(scenario_files)

    lower = float(args.lower_bound)
    upper = float(args.upper_bound)
    if not lower < upper:
        raise SystemExit("Lower bound must be strictly less than upper bound.")
    if args.initial_step <= 0:
        raise SystemExit("Initial step must be positive.")
    if args.min_step <= 0:
        raise SystemExit("Minimum step must be positive.")
    if args.refine_factor <= 1:
        raise SystemExit("Refine factor must be greater than 1.")
    if args.max_iters <= 0:
        raise SystemExit("Maximum iterations must be positive.")

    # Load platform JSON once (reused for every simulation)
    platform_json = platform_path.read_text()

    # Load all scenarios once (ScenarioData objects are reused)
    scenarios = [pyschedsim.load_scenario(str(f)) for f in scenario_files]

    evaluate_target.cache = {}  # type: ignore[attr-defined]

    print(
        f"Exploring targets between {lower:.6f} and {upper:.6f} across "
        f"{scenario_count} tasksets from {target_folder.relative_to(repo_root)}",
    )

    def generate_targets(lo: float, hi: float, step: float) -> Sequence[float]:
        assert step > 0
        values = []
        value = lo
        epsilon = step * 0.1 + 1e-12
        while value <= hi + epsilon:
            values.append(float(f"{value:.6f}"))
            value += step
        if not values or values[0] > lo + epsilon:
            values.insert(0, float(f"{lo:.6f}"))
        if values[-1] < hi - epsilon:
            values.append(float(f"{hi:.6f}"))
        # Deduplicate while preserving order
        seen = set()
        ordered = []
        for val in values:
            if val < lower:
                continue
            if val > upper:
                continue
            if val in seen:
                continue
            seen.add(val)
            ordered.append(val)
        return ordered

    step = float(args.initial_step)
    min_step = float(args.min_step)
    refine_factor = float(args.refine_factor)
    iterations = 0
    current_lower = lower
    current_upper = upper
    best_entry = None

    while iterations < args.max_iters:
        targets = generate_targets(current_lower, current_upper, step)
        if not targets:
            break
        for target in targets:
            entry = evaluate_target(
                target,
                scenarios,
                scenario_count,
                scenario_files,
                platform_json,
                args.scheduler,
                args.verbose,
            )
            if best_entry is None or entry["mean_share"] < best_entry["mean_share"] or (
                entry["mean_share"] == best_entry["mean_share"]
                and entry["target"] < best_entry["target"]
            ):
                best_entry = entry

        iterations += 1

        if step <= min_step + 1e-12:
            break

        prev_step = step
        step = max(step / refine_factor, min_step)
        assert best_entry is not None
        radius = max(prev_step, step)
        new_lower = max(lower, best_entry["target"] - radius)
        new_upper = min(upper, best_entry["target"] + radius)
        if new_upper - new_lower <= step / 2:
            break
        current_lower = new_lower
        current_upper = new_upper

    evaluated_entries = sorted(
        evaluate_target.cache.values(), key=lambda entry: entry["target"]  # type: ignore[attr-defined]
    )

    print("Target      Total Rejects    Mean Reject Share (%)")
    print("--------------------------------------------------")
    for entry in evaluated_entries:
        target = entry["target"]
        total = entry["total_rejects"]
        mean_share = entry["mean_share"] * 100
        print(f"{target:>8.6f}    {total:>13d}    {mean_share:>15.3f}%")

    best = min(
        evaluated_entries,
        key=lambda entry: (entry["mean_share"], entry["target"]),
    )

    print()
    print(
        f"Best target: {best['target']:.6f} with {best['total_rejects']} total rejected arrivals"
        f" ({best['mean_share'] * 100:.3f}% mean share)"
    )

    csv_name = f"ff_cap_search_{sanitize_for_filename(args.total_util)}.csv"
    csv_path = repo_root / csv_name
    with csv_path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "target",
                "total_rejects",
                "total_arrivals",
                "mean_reject_share",
                "overall_reject_share",
            ]
        )
        for entry in evaluated_entries:
            overall_share = (
                entry["total_rejects"] / entry["total_arrivals"]
                if entry["total_arrivals"]
                else 0.0
            )
            writer.writerow([
                f"{entry['target']:.6f}",
                entry["total_rejects"],
                entry["total_arrivals"],
                f"{entry['mean_share']:.6f}",
                f"{overall_share:.6f}",
            ])

    try:
        csv_display = csv_path.relative_to(repo_root)
    except ValueError:
        csv_display = csv_path
    print(f"Saved {len(evaluated_entries)} evaluations to {csv_display}")

    if args.verbose:
        print()
        print("Per-scenario rejects for best target:")
        for scenario, rejects, arrivals, share in best["per_scenario"]:
            label = format_scenario(scenario, repo_root)
            print(f"  {label}: {rejects} ({share * 100:.3f}% share of {arrivals} arrivals)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
