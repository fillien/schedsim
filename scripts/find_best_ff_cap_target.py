#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Dict, Optional, Sequence


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
        help="Step size for the coarse sweep. Default: 0.02",
    )
    parser.add_argument(
        "--min-step",
        type=float,
        default=0.00025,
        help="Stop refining once the step falls below this value. Default: 0.0025",
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
        help="Platform description passed to apps/alloc. Default: platforms/exynos5422.json",
    )
    parser.add_argument(
        "--scheduler",
        default="grub",
        help="Scheduler name passed to apps/alloc. Default: grub",
    )
    parser.add_argument(
        "--alloc-binary",
        default="build/apps/alloc",
        help="Path to the apps/alloc executable. Default: build/apps/alloc",
    )
    parser.add_argument(
        "--min-tasksets",
        default="min_tasksets",
        help="Base directory containing utilization folders. Default: min_tasksets",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print per-run output from apps/alloc.",
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


def read_rejects_from_csv(csv_path: Path) -> int:
    with csv_path.open(newline="") as handle:
        reader = csv.reader(handle, delimiter=";")
        rows = list(reader)
    if not rows:  # pragma: no cover - defensive
        raise RuntimeError(f"No rows written to {csv_path} by apps/alloc.")
    try:
        result_value = int(rows[-1][2])
    except (IndexError, ValueError) as exc:  # pragma: no cover - defensive
        raise RuntimeError(f"Unexpected CSV format in {csv_path}.") from exc
    return result_value


def run_alloc(
    binary: Path,
    platform: Path,
    scheduler: str,
    allocator: str,
    scenario: Path,
    repo_root: Path,
    verbose: bool,
    target: Optional[float] = None,
) -> int:
    target_str: Optional[str] = None
    if target is not None:
        target_formatted = float(f"{target:.6f}")
        target_str = f"{target_formatted:.6f}"
    with TemporaryDirectory(dir=repo_root) as temp_dir:
        temp_path = Path(temp_dir)
        cmd = [
            str(binary),
            "--platform",
            str(platform),
            "--sched",
            scheduler,
        ]
        cmd.extend(["--alloc", allocator])
        if target_str is not None:
            cmd.extend(["--target", target_str])
        cmd.extend(["--input", str(scenario)])
        completed = subprocess.run(
            cmd,
            cwd=temp_path,
            capture_output=True,
            text=True,
            check=False,
        )
        if verbose:
            if completed.stdout:
                sys.stdout.write(completed.stdout)
            if completed.stderr:
                sys.stdout.write(completed.stderr)
            if completed.stdout or completed.stderr:
                sys.stdout.flush()
        if completed.returncode != 0:
            raise RuntimeError(
                "apps/alloc failed for allocator "
                f"{allocator}"
                + (f" target {target_str}" if target_str is not None else "")
                + f" on {scenario}:\n"
                f"STDOUT: {completed.stdout}\nSTDERR: {completed.stderr}"
            )
        csv_path = temp_path / "min_taskset_result.csv"
        if not csv_path.exists():
            raise RuntimeError(
                "apps/alloc did not produce "
                f"{csv_path}"
                + (f" for target {target_str}" if target_str is not None else "")
                + "."
            )
        rejects = read_rejects_from_csv(csv_path)
    return rejects


def collect_total_arrivals(
    scenarios: Sequence[Path],
    binary: Path,
    platform: Path,
    scheduler: str,
    repo_root: Path,
    verbose: bool,
) -> Dict[Path, int]:
    totals: Dict[Path, int] = {}
    for scenario in scenarios:
        total = run_alloc(
            binary,
            platform,
            scheduler,
            "counting",
            scenario,
            repo_root,
            verbose,
        )
        totals[scenario] = total
    return totals


def evaluate_target(
    target: float,
    scenarios: Sequence[Path],
    scenario_totals: Dict[Path, int],
    scenario_count: int,
    binary: Path,
    platform: Path,
    scheduler: str,
    repo_root: Path,
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
    for scenario in scenarios:
        rejects = run_alloc(
            binary,
            platform,
            scheduler,
            "ff_cap",
            scenario,
            repo_root,
            verbose,
            target=target_formatted,
        )
        arrivals = scenario_totals[scenario]
        share = rejects / arrivals if arrivals else 0.0
        per_scenario.append((scenario, rejects, arrivals, share))
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

    binary_path = (repo_root / args.alloc_binary).resolve()
    if not binary_path.exists():
        raise SystemExit(f"apps/alloc binary not found at {binary_path}.")

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

    scenario_totals = collect_total_arrivals(
        scenario_files,
        binary_path,
        platform_path,
        args.scheduler,
        repo_root,
        args.verbose,
    )
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
                scenario_files,
                scenario_totals,
                scenario_count,
                binary_path,
                platform_path,
                args.scheduler,
                repo_root,
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
