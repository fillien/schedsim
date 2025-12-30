#!/usr/bin/env python3
"""
Generate a box plot showing acceptance rate gain per utilization level.
Gain = acceptance_rate(optimal_cap) - acceptance_rate(cap=1.0)
"""
from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Dict, List, Tuple
import glob

import matplotlib.pyplot as plt
import pandas as pd


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate box plot of acceptance rate gain.",
    )
    parser.add_argument(
        "--min-tasksets",
        required=True,
        help="Directory containing taskset folders and ff_cap_search_*.csv files.",
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
        "--verbose",
        action="store_true",
        help="Print progress information.",
    )
    return parser.parse_args()


def resolve_repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def read_rejects_from_csv(csv_path: Path) -> int:
    with csv_path.open(newline="") as handle:
        reader = csv.reader(handle, delimiter=";")
        rows = list(reader)
    if not rows:
        raise RuntimeError(f"No rows written to {csv_path} by apps/alloc.")
    try:
        result_value = int(rows[-1][2])
    except (IndexError, ValueError) as exc:
        raise RuntimeError(f"Unexpected CSV format in {csv_path}.") from exc
    return result_value


def run_alloc(
    binary: Path,
    platform: Path,
    scheduler: str,
    allocator: str,
    scenario: Path,
    repo_root: Path,
    target: float | None = None,
) -> int:
    """Run the simulator and return rejection count."""
    target_str = None
    if target is not None:
        target_formatted = float(f"{target:.6f}")
        target_str = f"{target_formatted:.6f}"

    with TemporaryDirectory(dir=repo_root) as temp_dir:
        temp_path = Path(temp_dir)
        cmd = [
            str(binary),
            "--platform", str(platform),
            "--sched", scheduler,
            "--alloc", allocator,
        ]
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
        if completed.returncode != 0:
            raise RuntimeError(
                f"apps/alloc failed for allocator {allocator}"
                + (f" target {target_str}" if target_str else "")
                + f" on {scenario}:\n"
                f"STDOUT: {completed.stdout}\nSTDERR: {completed.stderr}"
            )

        csv_path = temp_path / "min_taskset_result.csv"
        if not csv_path.exists():
            raise RuntimeError(f"apps/alloc did not produce {csv_path}.")
        return read_rejects_from_csv(csv_path)


def find_optimal_targets(tasksets_dir: Path) -> Dict[int, float]:
    """Read ff_cap_search_*.csv files and find optimal target for each utilization."""
    optimal_targets = {}
    csv_files = sorted(tasksets_dir.glob("ff_cap_search_*.csv"))

    for csv_file in csv_files:
        suffix = csv_file.stem.split("_")[-1]
        util_key = int(suffix)

        df = pd.read_csv(csv_file)
        min_idx = df["mean_reject_share"].idxmin()
        optimal_target = df.loc[min_idx, "target"]
        optimal_targets[util_key] = optimal_target

    return optimal_targets


def main() -> int:
    args = parse_args()
    repo_root = resolve_repo_root()

    binary_path = (repo_root / args.alloc_binary).resolve()
    if not binary_path.exists():
        raise SystemExit(f"apps/alloc binary not found at {binary_path}.")

    platform_path = (repo_root / args.platform).resolve()
    if not platform_path.exists():
        raise SystemExit(f"Platform file not found at {platform_path}.")

    tasksets_dir = Path(args.min_tasksets).resolve()
    if not tasksets_dir.is_dir():
        raise SystemExit(f"Taskset directory not found at {tasksets_dir}.")

    # Step 1: Find optimal targets from existing CSV files
    print("Finding optimal targets from ff_cap_search_*.csv files...")
    optimal_targets = find_optimal_targets(tasksets_dir)
    if not optimal_targets:
        raise SystemExit("No ff_cap_search_*.csv files found.")

    print(f"Found optimal targets for {len(optimal_targets)} utilization levels:")
    for util_key in sorted(optimal_targets.keys()):
        print(f"  U={util_key/10:.1f}: optimal cap = {optimal_targets[util_key]:.6f}")

    # Step 2: Run simulations for each utilization level
    results: List[Dict] = []

    for util_key in sorted(optimal_targets.keys()):
        optimal_cap = optimal_targets[util_key]
        util_folder = tasksets_dir / str(util_key)

        if not util_folder.is_dir():
            print(f"Warning: Folder {util_folder} not found, skipping.")
            continue

        scenario_files = sorted(util_folder.glob("*.json"))
        if not scenario_files:
            print(f"Warning: No JSON files in {util_folder}, skipping.")
            continue

        print(f"\nProcessing U={util_key/10:.1f} ({len(scenario_files)} tasksets)...")

        for i, scenario in enumerate(scenario_files):
            if args.verbose or (i + 1) % 20 == 0:
                print(f"  Taskset {i+1}/{len(scenario_files)}: {scenario.name}")

            # Get total arrivals using counting allocator
            total_arrivals = run_alloc(
                binary_path, platform_path, args.scheduler,
                "counting", scenario, repo_root
            )

            # Get rejections at optimal cap
            rejects_optimal = run_alloc(
                binary_path, platform_path, args.scheduler,
                "ff_cap", scenario, repo_root, target=optimal_cap
            )

            # Get rejections at cap=1.0 (baseline)
            rejects_baseline = run_alloc(
                binary_path, platform_path, args.scheduler,
                "ff_cap", scenario, repo_root, target=1.0
            )

            # Calculate acceptance rates
            if total_arrivals > 0:
                accept_optimal = 1 - (rejects_optimal / total_arrivals)
                accept_baseline = 1 - (rejects_baseline / total_arrivals)
            else:
                accept_optimal = 1.0
                accept_baseline = 1.0

            gain = accept_optimal - accept_baseline

            results.append({
                "utilization": util_key / 10,
                "taskset": scenario.name,
                "optimal_cap": optimal_cap,
                "total_arrivals": total_arrivals,
                "rejects_optimal": rejects_optimal,
                "rejects_baseline": rejects_baseline,
                "acceptance_optimal": accept_optimal,
                "acceptance_baseline": accept_baseline,
                "gain": gain,
            })

    if not results:
        raise SystemExit("No results collected.")

    # Step 3: Create DataFrame and export
    df = pd.DataFrame(results)

    csv_output = tasksets_dir / "acceptance_gain_data.csv"
    df.to_csv(csv_output, index=False)
    print(f"\nExported data to {csv_output}")

    # Step 4: Create box plot
    plt.figure(figsize=(12, 6))

    utilizations = sorted(df["utilization"].unique())
    data_for_boxplot = [df[df["utilization"] == u]["gain"].values for u in utilizations]

    bp = plt.boxplot(data_for_boxplot, labels=[f"{u:.1f}" for u in utilizations], patch_artist=True)

    for patch in bp['boxes']:
        patch.set_facecolor('#2ca02c')
        patch.set_alpha(0.7)

    plt.xlabel("Total Utilization")
    plt.ylabel("Acceptance Rate Gain")
    plt.title("Acceptance Rate Gain (Optimal Cap vs Cap=1.0) by Utilization Level")
    plt.grid(True, axis='y', alpha=0.3)
    plt.axhline(y=0, color='red', linestyle='--', alpha=0.5, label='No gain')
    plt.legend()
    plt.tight_layout()

    plot_output = tasksets_dir / "acceptance_gain_boxplot.png"
    plt.savefig(plot_output, dpi=300)
    plt.close()
    print(f"Saved plot to {plot_output}")

    # Print summary statistics
    print("\nSummary statistics:")
    print(df.groupby("utilization")["gain"].describe())

    return 0


if __name__ == "__main__":
    sys.exit(main())
