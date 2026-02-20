#!/usr/bin/env python3
"""
Evaluate the ff_cap_adaptive_linear allocator against ff_little_first baseline.
Generates gains bar charts and rejected utilization histograms for each taskset folder.
"""

import subprocess
import json
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import argparse

BASE_DIR = Path(__file__).parent.parent
ALLOC_BIN = str(BASE_DIR / "build/apps/alloc")
PLATFORM = str(BASE_DIR / "platforms/exynos5422.json")
NB_JOBS = 100
UTIL_STEPS = list(reversed(range(21, 66, 2)))


def run_simulation(taskset_file, alloc, reclaim="grub"):
    """Run a single simulation and return (arrivals, rejected_count, rejected_utils)"""
    cmd = [ALLOC_BIN, "--platform", PLATFORM,
           "--reclaim", reclaim, "--alloc", alloc,
           "--input", str(taskset_file)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode == 0 and result.stdout.strip():
        parts = result.stdout.strip().split(";")
        rejected_count = int(parts[2]) if len(parts) >= 3 else 0
        with open(taskset_file) as tf:
            taskset = json.load(tf)
            arrivals = sum(len(t["jobs"]) for t in taskset["tasks"])
        # Rejected utilization list is not available from new alloc stdout
        return arrivals, rejected_count, []
    return 0, 0, []


def collect_data(taskset_dir):
    """Collect data from a taskset folder without generating plots"""
    allocators = ["ff_little_first", "ff_cap_adaptive_linear"]
    results = {alloc: {} for alloc in allocators}
    all_rejected_utils = {alloc: [] for alloc in allocators}

    for util_level in UTIL_STEPS:
        level_dir = Path(taskset_dir) / str(util_level)
        if not level_dir.exists():
            continue

        for alloc in allocators:
            total_arrivals = 0
            total_rejected = 0

            for i in range(1, NB_JOBS + 1):
                taskset_file = level_dir / f"{i}.json"
                if taskset_file.exists():
                    arrivals, rejected, rejected_utils = run_simulation(taskset_file, alloc)
                    total_arrivals += arrivals
                    total_rejected += rejected
                    all_rejected_utils[alloc].extend(rejected_utils)

            if total_arrivals > 0:
                accept_rate = (total_arrivals - total_rejected) / total_arrivals * 100
            else:
                accept_rate = 0
            results[alloc][util_level] = accept_rate

    # Compute gains
    util_levels = sorted(results["ff_little_first"].keys())
    gains = []

    for level in util_levels:
        baseline = results["ff_little_first"].get(level, 0)
        adaptive = results["ff_cap_adaptive_linear"].get(level, 0)
        gain = adaptive - baseline
        gains.append(gain)

    return results, gains, all_rejected_utils, util_levels


def generate_plots(output_prefix, output_dir, suffix, results, gains, all_rejected_utils, util_levels, gains_ylim, rejected_ylim):
    """Generate plots for a taskset folder with specified y-axis limits"""
    x_labels = [f"{l/10:.1f}" for l in util_levels]
    mean_gain = np.mean(gains) if gains else 0

    # Plot 1: Gains bar chart
    fig1, ax1 = plt.subplots(figsize=(12, 6))
    colors = ['green' if g >= 0 else 'red' for g in gains]
    ax1.bar(x_labels, gains, color=colors, edgecolor='black', alpha=0.7)
    ax1.axhline(y=mean_gain, color='blue', linestyle='--', label=f'Mean: {mean_gain:.2f} pp')
    ax1.axhline(y=0, color='black', linestyle='-', linewidth=0.5)
    ax1.set_xlabel('Total Utilization')
    ax1.set_ylabel('Gain (percentage points)')
    ax1.set_title(f'Acceptance Rate Gains: ff_cap_adaptive_linear vs ff_little_first\n({output_prefix})')
    ax1.legend()
    ax1.tick_params(axis='x', rotation=45)
    ax1.set_ylim(gains_ylim)
    plt.tight_layout()

    gains_file = output_dir / f'gains_{output_prefix}{suffix}.png'
    plt.savefig(gains_file, dpi=150)
    plt.close()
    print(f"  Saved {gains_file}")

    # Plot 2: Rejected utilizations histogram (both allocators side-by-side)
    rejected_adaptive = all_rejected_utils["ff_cap_adaptive_linear"]
    rejected_baseline = all_rejected_utils["ff_little_first"]

    if rejected_adaptive or rejected_baseline:
        fig2, ax2 = plt.subplots(figsize=(14, 6))

        # Create histogram counts for both
        n_bins = 30
        bins = np.linspace(0, 1, n_bins + 1)

        counts_baseline, _ = np.histogram(rejected_baseline, bins=bins) if rejected_baseline else (np.zeros(n_bins), None)
        counts_adaptive, _ = np.histogram(rejected_adaptive, bins=bins) if rejected_adaptive else (np.zeros(n_bins), None)

        # Compute bar positions
        bin_centers = (bins[:-1] + bins[1:]) / 2
        bar_width = (bins[1] - bins[0]) * 0.4

        # Plot side-by-side bars
        ax2.bar(bin_centers - bar_width/2, counts_baseline, width=bar_width,
                color='coral', edgecolor='black', alpha=0.8,
                label=f'ff_little_first ({len(rejected_baseline)})')
        ax2.bar(bin_centers + bar_width/2, counts_adaptive, width=bar_width,
                color='steelblue', edgecolor='black', alpha=0.8,
                label=f'ff_cap_adaptive_linear ({len(rejected_adaptive)})')

        ax2.set_xlabel('Task Utilization')
        ax2.set_ylabel('Rejected Jobs')
        ax2.set_title(f'Rejected Jobs by Task Utilization\n({output_prefix})')
        ax2.set_xlim(0, 1)
        ax2.set_ylim(rejected_ylim)
        ax2.legend()
        plt.tight_layout()

        rejected_file = output_dir / f'rejected_utils_{output_prefix}{suffix}.png'
        plt.savefig(rejected_file, dpi=150)
        plt.close()
        print(f"  Saved {rejected_file}")

    print(f"  Mean gain: {mean_gain:.2f} pp")
    print(f"  Positive gains: {sum(1 for g in gains if g > 0)}/{len(gains)}")
    print(f"  Total rejected (adaptive): {len(all_rejected_utils['ff_cap_adaptive_linear'])}")


def main():
    parser = argparse.ArgumentParser(description="Evaluate ff_cap_adaptive_linear allocator")
    parser.add_argument("--suffix", type=str, default="", help="Suffix for output files (e.g., '_intercept')")
    parser.add_argument("--output-dir", type=Path, default=BASE_DIR, help="Output directory for plots")
    parser.add_argument("--folders", nargs="+", default=["min_tasksets_little_task", "min_tasksets_big_task", "min_tasksets_mid_task"],
                        help="Taskset folders to evaluate")
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)

    # First pass: collect data from all folders
    all_data = {}
    for folder in args.folders:
        folder_path = BASE_DIR / folder
        if folder_path.exists():
            prefix = folder.replace("min_tasksets_", "")
            print(f"Collecting data from {prefix}...")
            results, gains, all_rejected_utils, util_levels = collect_data(folder_path)
            all_data[prefix] = (results, gains, all_rejected_utils, util_levels)
        else:
            print(f"Folder {folder} not found, skipping...")

    if not all_data:
        print("No data collected!")
        return

    # Compute global y-axis limits
    all_gains = [g for data in all_data.values() for g in data[1]]
    gains_min = min(all_gains) if all_gains else 0
    gains_max = max(all_gains) if all_gains else 0
    gains_margin = (gains_max - gains_min) * 0.1
    gains_ylim = (gains_min - gains_margin, gains_max + gains_margin)

    # Compute global rejected histogram y-limit
    n_bins = 30
    bins = np.linspace(0, 1, n_bins + 1)
    max_count = 0
    for data in all_data.values():
        all_rejected_utils = data[2]
        for alloc in all_rejected_utils:
            if all_rejected_utils[alloc]:
                counts, _ = np.histogram(all_rejected_utils[alloc], bins=bins)
                max_count = max(max_count, counts.max())
    rejected_ylim = (0, max_count * 1.1)

    # Second pass: generate plots with consistent y-axis limits
    for prefix, (results, gains, all_rejected_utils, util_levels) in all_data.items():
        print(f"Generating plots for {prefix}...")
        generate_plots(prefix, args.output_dir, args.suffix, results, gains, all_rejected_utils, util_levels, gains_ylim, rejected_ylim)

    print("\nAll tests completed!")


if __name__ == "__main__":
    main()
