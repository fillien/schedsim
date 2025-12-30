#!/usr/bin/env python3
"""Benchmark ff_cap_adaptive_linear allocator against baseline.

Generates a CSV with per-taskset absolute gains for statistical analysis.

Usage:
    python scripts/benchmark_adaptive_linear.py \
        --taskset-dir min_tasksets_umax_0133_umin_0025_tasks_55 \
        --output linear_model_gains.csv
"""

import argparse
import csv
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Benchmark ff_cap_adaptive_linear allocator against baseline.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--taskset-dir",
        type=Path,
        required=True,
        help="Directory containing utilization folders with taskset JSONs.",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=Path("linear_model_gains.csv"),
        help="Output CSV file. Default: linear_model_gains.csv",
    )
    parser.add_argument(
        "--baseline",
        type=str,
        default="ff_little_first",
        help="Baseline allocator. Default: ff_little_first",
    )
    parser.add_argument(
        "--model",
        type=str,
        default="ff_cap_adaptive_linear",
        help="Model allocator to test. Default: ff_cap_adaptive_linear",
    )
    parser.add_argument(
        "--num-tasksets",
        type=int,
        default=50,
        help="Number of tasksets per utilization level. Default: 50",
    )
    parser.add_argument(
        "--platform",
        type=Path,
        default=None,
        help="Platform JSON file. Default: platforms/exynos5422.json",
    )
    return parser.parse_args()


def find_repo_root() -> Path:
    """Find repository root by looking for build/apps/alloc."""
    current = Path(__file__).resolve().parent
    while current != current.parent:
        if (current / "build" / "apps" / "alloc").exists():
            return current
        current = current.parent
    raise RuntimeError("Could not find repository root")


def run_allocator(binary: Path, platform: Path, taskset_path: Path, allocator: str) -> int | None:
    """Run allocator and return rejection count."""
    with TemporaryDirectory() as tmpdir:
        cmd = [
            str(binary),
            "--platform", str(platform),
            "--sched", "grub",
            "--alloc", allocator,
            "--input", str(taskset_path),
        ]
        subprocess.run(cmd, cwd=tmpdir, capture_output=True)
        csv_path = Path(tmpdir) / "min_taskset_result.csv"
        if csv_path.exists():
            with open(csv_path) as f:
                rows = list(csv.reader(f, delimiter=";"))
                if rows:
                    return int(rows[-1][2])
    return None


def main() -> int:
    args = parse_args()

    repo_root = find_repo_root()
    binary = repo_root / "build" / "apps" / "alloc"
    platform = args.platform or (repo_root / "platforms" / "exynos5422.json")
    taskset_dir = args.taskset_dir
    if not taskset_dir.is_absolute():
        taskset_dir = repo_root / taskset_dir

    if not binary.exists():
        print(f"Error: binary not found: {binary}")
        return 1
    if not platform.exists():
        print(f"Error: platform not found: {platform}")
        return 1
    if not taskset_dir.is_dir():
        print(f"Error: taskset directory not found: {taskset_dir}")
        return 1

    print(f"Binary: {binary}")
    print(f"Platform: {platform}")
    print(f"Taskset dir: {taskset_dir}")
    print(f"Baseline: {args.baseline}")
    print(f"Model: {args.model}")
    print()

    results = []
    for util_folder in sorted(taskset_dir.iterdir()):
        if not util_folder.is_dir():
            continue
        try:
            util_code = int(util_folder.name)
        except ValueError:
            continue

        total_util = util_code / 10.0
        tasksets = sorted(util_folder.glob("*.json"))[: args.num_tasksets]

        for ts in tasksets:
            # Get arrivals
            arrivals = run_allocator(binary, platform, ts, "counting")
            if arrivals is None or arrivals == 0:
                continue

            # Get baseline rejections
            baseline_rej = run_allocator(binary, platform, ts, args.baseline)
            if baseline_rej is None:
                continue

            # Get model rejections
            model_rej = run_allocator(binary, platform, ts, args.model)
            if model_rej is None:
                continue

            # Calculate rejection rates and gain
            baseline_rate = baseline_rej / arrivals * 100
            model_rate = model_rej / arrivals * 100
            gain = baseline_rate - model_rate

            results.append({
                "util": total_util,
                "taskset": ts.stem,
                "arrivals": arrivals,
                "baseline_rej": baseline_rej,
                "model_rej": model_rej,
                "baseline_rate": baseline_rate,
                "model_rate": model_rate,
                "gain_pp": gain,
            })
            print(f"U={total_util:.1f} {ts.stem}: {gain:+.2f} pp")

    if not results:
        print("Error: no results collected")
        return 1

    # Write CSV
    with open(args.output, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=results[0].keys())
        writer.writeheader()
        writer.writerows(results)

    print(f"\nCSV saved to: {args.output}")
    print(f"Total samples: {len(results)}")

    return 0


if __name__ == "__main__":
    exit(main())
