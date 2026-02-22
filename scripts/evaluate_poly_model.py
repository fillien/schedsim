#!/usr/bin/env python3
"""Evaluate the polynomial model on out-of-sample taskset directories.

Compares the polynomial-predicted target vs the grid-search optimal target
for each (directory, utilization level) pair. Runs a simulation at the
polynomial-predicted target to measure the actual rejection rate.

Usage:
    python scripts/evaluate_poly_model.py \
        --test-dirs min_tasksets_big_task min_tasksets_little_task min_tasksets_mid_task \
        --train-dirs min_tasksets_umax_*
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path
import numpy as np

# ---------------------------------------------------------------------------
# Reuse helpers from sibling scripts (they live in the same directory)
# ---------------------------------------------------------------------------
sys.path.insert(0, str(Path(__file__).resolve().parent))

from fit_linear_model import compute_umax_for_utilization, find_optimal_target
from find_best_ff_cap_target import (
    run_simulation,
    evaluate_target,
)

# ---------------------------------------------------------------------------
# Polynomial coefficients (must match ff_cap_adaptive_poly_allocator.cpp)
# ---------------------------------------------------------------------------
C0 = -3.556483715
C1 =  0.465435461
C2 =  1.273160775
C3 = -2.805216317
C4 =  0.651635123
C5 = -0.112785097

TRAIN_UMAX_MAX = 0.30  # boundary between interpolation and extrapolation


def poly_predict(umax: float, U: float) -> float:
    raw = C0 + C1 * umax + C2 * U + C3 * umax**2 + C4 * umax * U + C5 * U**2
    return max(0.0, min(1.0, raw))


def resolve_repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


UTIL_CODES = [21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63, 65]


def run_at_target(
    target: float,
    scenario_files: list[Path],
    platform_json: str,
    scheduler: str,
) -> float:
    """Run simulations at a specific target and return mean rejection share."""
    import pyschedsim

    scenarios = [pyschedsim.load_scenario(str(f)) for f in scenario_files]

    sum_share = 0.0
    for scenario in scenarios:
        rejects, arrivals = run_simulation(platform_json, scenario, scheduler, target=target)
        share = rejects / arrivals if arrivals else 0.0
        sum_share += share

    return sum_share / len(scenarios) if scenarios else 0.0


def evaluate_directory(
    taskset_dir: Path,
    platform_json: str,
    scheduler: str,
) -> list[dict]:
    """Evaluate all utilization levels in a single test directory."""
    rows = []
    for uc in UTIL_CODES:
        folder = taskset_dir / str(uc)
        if not folder.is_dir():
            continue

        csv_path = taskset_dir / f"ff_cap_search_{uc}.csv"
        if not csv_path.exists():
            print(f"  Warning: no CSV for U={uc/10:.1f} in {taskset_dir.name}, skipping")
            continue

        U = uc / 10.0
        umax = compute_umax_for_utilization(taskset_dir, uc)
        poly_target = poly_predict(umax, U)
        optimal_target, optimal_reject = find_optimal_target(csv_path)

        # Run simulation at the polynomial-predicted target
        scenario_files = sorted(folder.glob("*.json"))
        if not scenario_files:
            continue

        # Reset the evaluate_target cache between runs
        evaluate_target.cache = {}

        poly_reject = run_at_target(poly_target, scenario_files, platform_json, scheduler)

        excess = (poly_reject - optimal_reject) * 100  # in percentage points
        regime = "interpolation" if umax <= TRAIN_UMAX_MAX else "extrapolation"

        rows.append({
            "dir": taskset_dir.name,
            "U": U,
            "umax": umax,
            "poly_target": poly_target,
            "optimal_target": optimal_target,
            "poly_reject_pct": poly_reject * 100,
            "optimal_reject_pct": optimal_reject * 100,
            "excess_reject_pp": excess,
            "target_error": abs(poly_target - optimal_target),
            "regime": regime,
        })
        print(f"  U={U:.1f}  umax={umax:.4f}  poly={poly_target:.4f}  opt={optimal_target:.4f}  "
              f"poly_rej={poly_reject*100:.2f}%  opt_rej={optimal_reject*100:.2f}%  "
              f"excess={excess:+.2f}pp  [{regime}]")

    return rows


def collect_training_points(train_dirs: list[Path]) -> list[dict]:
    """Collect (umax, U) points from training directories for the domain plot."""
    points = []
    for d in sorted(train_dirs):
        if not d.is_dir():
            continue
        for uc in UTIL_CODES:
            folder = d / str(uc)
            if not folder.is_dir():
                continue
            try:
                umax = compute_umax_for_utilization(d, uc)
            except (FileNotFoundError, ValueError):
                continue
            points.append({"umax": umax, "U": uc / 10.0, "source": d.name, "regime": "training"})
    return points


def print_table(rows: list[dict]) -> None:
    header = (
        f"{'Dir':<30s} {'U':>4s} {'umax':>7s} {'poly_t':>7s} {'opt_t':>7s} "
        f"{'poly_rej%':>9s} {'opt_rej%':>9s} {'excess':>8s} {'regime':<15s}"
    )
    print(header)
    print("-" * len(header))
    for r in rows:
        print(
            f"{r['dir']:<30s} {r['U']:>4.1f} {r['umax']:>7.4f} {r['poly_target']:>7.4f} "
            f"{r['optimal_target']:>7.4f} {r['poly_reject_pct']:>8.2f}% {r['optimal_reject_pct']:>8.2f}% "
            f"{r['excess_reject_pp']:>+7.2f}pp {r['regime']:<15s}"
        )


def print_aggregate(rows: list[dict]) -> None:
    if not rows:
        return

    interp = [r for r in rows if r["regime"] == "interpolation"]
    extrap = [r for r in rows if r["regime"] == "extrapolation"]

    print(f"\n{'Aggregate Metrics':=^70}")
    for label, subset in [("All", rows), ("Interpolation", interp), ("Extrapolation", extrap)]:
        if not subset:
            print(f"\n  {label}: no data points")
            continue
        target_errors = [r["target_error"] for r in subset]
        excess = [r["excess_reject_pp"] for r in subset]
        print(f"\n  {label} ({len(subset)} points):")
        print(f"    Target MAE:           {np.mean(target_errors):.4f}")
        print(f"    Target max error:     {max(target_errors):.4f}")
        print(f"    Mean excess reject:   {np.mean(excess):+.2f} pp")
        print(f"    Max excess reject:    {max(excess):+.2f} pp")
        print(f"    Mean poly reject%:    {np.mean([r['poly_reject_pct'] for r in subset]):.2f}%")
        print(f"    Mean optimal reject%: {np.mean([r['optimal_reject_pct'] for r in subset]):.2f}%")


def save_csv(rows: list[dict], path: Path) -> None:
    fieldnames = [
        "dir", "U", "umax", "poly_target", "optimal_target",
        "poly_reject_pct", "optimal_reject_pct", "excess_reject_pp",
        "target_error", "regime",
    ]
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    print(f"\nResults saved to {path}")


# ---------------------------------------------------------------------------
# Plotting
# ---------------------------------------------------------------------------

def plot_rejection_comparison(rows: list[dict], output_path: Path) -> None:
    """Plot A: rejection rate vs U for polynomial vs optimal, one subplot per directory."""
    import matplotlib.pyplot as plt

    dirs = sorted(set(r["dir"] for r in rows))
    fig, axes = plt.subplots(1, len(dirs), figsize=(6 * len(dirs), 5), sharey=True, squeeze=False)

    for ax, dirname in zip(axes[0], dirs):
        subset = sorted([r for r in rows if r["dir"] == dirname], key=lambda r: r["U"])
        Us = [r["U"] for r in subset]
        poly_rej = [r["poly_reject_pct"] for r in subset]
        opt_rej = [r["optimal_reject_pct"] for r in subset]

        ax.plot(Us, opt_rej, "o-", label="Optimal (grid search)", color="#2196F3", markersize=4)
        ax.plot(Us, poly_rej, "s--", label="Polynomial model", color="#F44336", markersize=4)
        ax.fill_between(Us, opt_rej, poly_rej, alpha=0.15, color="#F44336")

        ax.set_xlabel("Total utilization U")
        ax.set_title(dirname.replace("min_tasksets_", "").replace("_", " ").title())
        ax.legend(fontsize=8)
        ax.grid(True, alpha=0.3)

    axes[0][0].set_ylabel("Mean rejection rate (%)")
    fig.suptitle("Polynomial model vs optimal target — out-of-sample evaluation", fontsize=12)
    fig.tight_layout()
    fig.savefig(output_path, dpi=150)
    print(f"Plot A saved to {output_path}")
    plt.close(fig)


def plot_domain(train_points: list[dict], test_rows: list[dict], output_path: Path) -> None:
    """Plot B: training vs test domain scatter in (umax, U) space."""
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(8, 6))

    # Training points
    if train_points:
        t_umax = [p["umax"] for p in train_points]
        t_U = [p["U"] for p in train_points]
        ax.scatter(t_umax, t_U, c="#2196F3", s=20, alpha=0.6, label="Training", zorder=2)

    # Test points, colored by directory
    colors = {"min_tasksets_big_task": "#F44336", "min_tasksets_little_task": "#4CAF50", "min_tasksets_mid_task": "#FF9800"}
    for r in test_rows:
        c = colors.get(r["dir"], "#999999")
        ax.scatter(r["umax"], r["U"], c=c, s=30, marker="^", alpha=0.8, zorder=3)

    # Add legend entries for test dirs
    for dirname, color in colors.items():
        label = dirname.replace("min_tasksets_", "").replace("_", " ").title()
        ax.scatter([], [], c=color, s=30, marker="^", label=f"Test: {label}")

    # Extrapolation boundary
    ax.axvline(x=TRAIN_UMAX_MAX, color="gray", linestyle=":", alpha=0.7, label=f"Training umax max ({TRAIN_UMAX_MAX})")

    ax.set_xlabel("umax (avg max per-task utilization)")
    ax.set_ylabel("Total utilization U")
    ax.set_title("Training vs test domain in (umax, U) space")
    ax.legend(fontsize=8, loc="upper left")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_path, dpi=150)
    print(f"Plot B saved to {output_path}")
    plt.close(fig)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Evaluate polynomial model on out-of-sample tasksets.")
    parser.add_argument(
        "--test-dirs", nargs="+", type=Path, required=True,
        help="Test directories (e.g. min_tasksets_big_task min_tasksets_little_task min_tasksets_mid_task).",
    )
    parser.add_argument(
        "--train-dirs", nargs="*", type=Path, default=[],
        help="Training directories (e.g. min_tasksets_umax_*) for domain plot.",
    )
    parser.add_argument(
        "--platform", default="platforms/exynos5422.json",
        help="Platform JSON. Default: platforms/exynos5422.json",
    )
    parser.add_argument("--scheduler", default="grub", help="Scheduler: grub or cash. Default: grub")
    parser.add_argument("--output-dir", type=Path, default=None, help="Directory for output files. Default: repo root.")
    parser.add_argument("--no-plots", action="store_true", help="Skip plot generation.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = resolve_repo_root()

    platform_path = (repo_root / args.platform).resolve()
    if not platform_path.exists():
        print(f"Error: platform file not found: {platform_path}")
        return 1
    platform_json = platform_path.read_text()

    output_dir = (args.output_dir or repo_root).resolve()

    all_rows: list[dict] = []
    for test_dir in args.test_dirs:
        d = (repo_root / test_dir).resolve() if not test_dir.is_absolute() else test_dir.resolve()
        if not d.is_dir():
            print(f"Warning: {d} not found, skipping")
            continue
        print(f"\n=== Evaluating {d.name} ===")
        rows = evaluate_directory(d, platform_json, args.scheduler)
        all_rows.extend(rows)

    if not all_rows:
        print("No results collected.")
        return 1

    print(f"\n{'Full comparison table':=^70}")
    print_table(all_rows)
    print_aggregate(all_rows)

    save_csv(all_rows, output_dir / "poly_model_evaluation.csv")

    if not args.no_plots:
        try:
            import matplotlib
            matplotlib.use("Agg")

            plot_rejection_comparison(all_rows, output_dir / "poly_eval_rejection.png")

            train_points = []
            if args.train_dirs:
                train_dirs = [
                    (repo_root / d).resolve() if not d.is_absolute() else d.resolve()
                    for d in args.train_dirs
                ]
                train_points = collect_training_points(train_dirs)

            plot_domain(train_points, all_rows, output_dir / "poly_eval_domain.png")
        except ImportError:
            print("Warning: matplotlib not available, skipping plots")

    return 0


if __name__ == "__main__":
    sys.exit(main())
