#!/usr/bin/env python3
"""
Fit a linear model for adaptive allocator threshold prediction.

Reads optimal target search results and fits: target = A*umax + B*U + C

Usage:
    python scripts/fit_linear_model.py \
        --taskset-dir min_tasksets_umax_0133_umin_0025_tasks_55 \
        --cpp-format
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

import numpy as np


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Fit linear model for adaptive allocator threshold.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--taskset-dir",
        type=Path,
        required=True,
        help="Directory containing utilization folders with taskset JSONs and search CSVs.",
    )
    parser.add_argument(
        "--search-dir",
        type=Path,
        default=None,
        help="Directory containing ff_cap_search_*.csv files. Defaults to taskset-dir.",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=None,
        help="Output file for coefficients. If not specified, prints to stdout.",
    )
    parser.add_argument(
        "--cpp-format",
        action="store_true",
        help="Output coefficients in C++ constexpr format.",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Print per-utilization training data.",
    )
    parser.add_argument(
        "--min-target",
        type=float,
        default=0.0,
        help="Exclude data points with optimal target <= this value (degenerate cases). Default: 0.0",
    )
    parser.add_argument(
        "--no-umax",
        action="store_true",
        help="Fit simpler model target = B*U + C (ignoring umax feature).",
    )
    parser.add_argument(
        "--compare",
        action="store_true",
        help="Compare both models (with and without umax) and show results.",
    )
    return parser.parse_args()


def compute_umax_for_utilization(taskset_dir: Path, util_code: int) -> float:
    """Compute the average maximum task utilization across all tasksets at a given utilization level."""
    folder = taskset_dir / str(util_code)
    if not folder.is_dir():
        raise FileNotFoundError(f"Utilization folder not found: {folder}")

    max_utils = []
    for taskset_file in sorted(folder.glob("*.json")):
        with open(taskset_file) as f:
            data = json.load(f)
        utils = [task["utilization"] for task in data["tasks"]]
        if utils:
            max_utils.append(max(utils))

    if not max_utils:
        raise ValueError(f"No tasksets found in {folder}")

    return sum(max_utils) / len(max_utils)


def find_optimal_target(csv_path: Path) -> tuple[float, float]:
    """Find the target with minimum mean_reject_share from a search CSV.

    Returns (optimal_target, min_reject_share).
    """
    with open(csv_path, newline="") as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    if not rows:
        raise ValueError(f"Empty CSV: {csv_path}")

    best = min(rows, key=lambda r: (float(r["mean_reject_share"]), float(r["target"])))
    return float(best["target"]), float(best["mean_reject_share"])


def collect_training_data(
    taskset_dir: Path, search_dir: Path, verbose: bool = False, min_target: float = 0.0
) -> tuple[np.ndarray, np.ndarray, list[dict]]:
    """Collect (umax, U) features and optimal target labels.

    Returns (features, targets, metadata).
    """
    features = []
    targets = []
    metadata = []
    skipped_count = 0

    csv_files = sorted(search_dir.glob("ff_cap_search_*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"No ff_cap_search_*.csv files found in {search_dir}")

    for csv_file in csv_files:
        # Extract utilization code from filename
        stem = csv_file.stem  # e.g., "ff_cap_search_47"
        util_code_str = stem.split("_")[-1]
        try:
            util_code = int(util_code_str)
        except ValueError:
            print(f"Warning: skipping {csv_file} - cannot parse utilization code")
            continue

        total_util = util_code / 10.0

        try:
            umax = compute_umax_for_utilization(taskset_dir, util_code)
        except FileNotFoundError:
            print(f"Warning: skipping U={total_util} - taskset folder {util_code} not found")
            continue

        optimal_target, reject_share = find_optimal_target(csv_file)

        # Skip degenerate data points
        if optimal_target <= min_target:
            if verbose:
                print(f"U={total_util:.1f}: umax={umax:.4f}, target={optimal_target:.6f} (SKIPPED - degenerate)")
            skipped_count += 1
            continue

        features.append([umax, total_util])
        targets.append(optimal_target)
        metadata.append({
            "util_code": util_code,
            "total_util": total_util,
            "umax": umax,
            "optimal_target": optimal_target,
            "reject_share": reject_share,
        })

        if verbose:
            print(f"U={total_util:.1f}: umax={umax:.4f}, target={optimal_target:.6f}, reject={reject_share:.4f}")

    if skipped_count > 0:
        print(f"Skipped {skipped_count} degenerate data points (optimal target <= {min_target})")

    return np.array(features), np.array(targets), metadata


def fit_model(X: np.ndarray, y: np.ndarray, use_umax: bool = True) -> dict:
    """Fit linear regression using least squares and return metrics.

    If use_umax is True, fits: target = A*umax + B*U + C
    If use_umax is False, fits: target = B*U + C
    """
    if use_umax:
        # Add column of ones for intercept
        X_with_intercept = np.column_stack([X, np.ones(len(X))])
        coeffs, residuals, rank, s = np.linalg.lstsq(X_with_intercept, y, rcond=None)
        coef_umax, coef_U, intercept = coeffs
    else:
        # Only use U (second column) + intercept
        U_with_intercept = np.column_stack([X[:, 1], np.ones(len(X))])
        coeffs, residuals, rank, s = np.linalg.lstsq(U_with_intercept, y, rcond=None)
        coef_umax = 0.0
        coef_U, intercept = coeffs

    # Compute predictions
    if use_umax:
        y_pred = np.column_stack([X, np.ones(len(X))]) @ [coef_umax, coef_U, intercept]
    else:
        y_pred = np.column_stack([X[:, 1], np.ones(len(X))]) @ [coef_U, intercept]

    # R² score
    ss_res = np.sum((y - y_pred) ** 2)
    ss_tot = np.sum((y - np.mean(y)) ** 2)
    r2 = 1 - (ss_res / ss_tot) if ss_tot > 0 else 0.0

    # Mean Absolute Error
    mae = np.mean(np.abs(y - y_pred))

    metrics = {
        "r2": r2,
        "mae": mae,
        "coef_umax": coef_umax,
        "coef_U": coef_U,
        "intercept": intercept,
    }

    return metrics


def format_cpp_output(metrics: dict) -> str:
    """Format coefficients as C++ constexpr declarations."""
    return f"""// Linear model coefficients: target = A*umax + B*U + C
// R² = {metrics['r2']:.4f}, MAE = {metrics['mae']:.4f}
constexpr double A_UMAX = {metrics['coef_umax']:.9f};
constexpr double B_U = {metrics['coef_U']:.9f};
constexpr double C_INTERCEPT = {metrics['intercept']:.9f};"""


def main() -> int:
    args = parse_args()

    taskset_dir = args.taskset_dir.resolve()
    if not taskset_dir.is_dir():
        print(f"Error: taskset directory not found: {taskset_dir}")
        return 1

    search_dir = (args.search_dir or args.taskset_dir).resolve()
    if not search_dir.is_dir():
        print(f"Error: search directory not found: {search_dir}")
        return 1

    print(f"Taskset directory: {taskset_dir}")
    print(f"Search directory: {search_dir}")
    print()

    print("Collecting training data...")
    try:
        X, y, metadata = collect_training_data(taskset_dir, search_dir, args.verbose, args.min_target)
    except FileNotFoundError as e:
        print(f"Error: {e}")
        return 1

    if len(y) < 3:
        print(f"Error: need at least 3 data points, got {len(y)}")
        return 1

    print(f"\nFitting model on {len(y)} data points...")
    print(f"  U range: [{min(m['total_util'] for m in metadata):.1f}, {max(m['total_util'] for m in metadata):.1f}]")
    print(f"  umax range: [{min(m['umax'] for m in metadata):.4f}, {max(m['umax'] for m in metadata):.4f}]")
    print(f"  target range: [{min(y):.4f}, {max(y):.4f}]")

    use_umax = not args.no_umax

    if args.compare:
        # Compare both models
        metrics_with_umax = fit_model(X, y, use_umax=True)
        metrics_without_umax = fit_model(X, y, use_umax=False)

        print()
        print("=" * 60)
        print("Model Comparison:")
        print()
        print("With umax (target = A*umax + B*U + C):")
        print(f"  target = {metrics_with_umax['coef_umax']:.6f}*umax + {metrics_with_umax['coef_U']:.6f}*U + ({metrics_with_umax['intercept']:.6f})")
        print(f"  R² = {metrics_with_umax['r2']:.4f}, MAE = {metrics_with_umax['mae']:.4f}")
        print()
        print("Without umax (target = B*U + C):")
        print(f"  target = {metrics_without_umax['coef_U']:.6f}*U + ({metrics_without_umax['intercept']:.6f})")
        print(f"  R² = {metrics_without_umax['r2']:.4f}, MAE = {metrics_without_umax['mae']:.4f}")
        print("=" * 60)

        # Use the model with umax for output
        metrics = metrics_with_umax if use_umax else metrics_without_umax
    else:
        metrics = fit_model(X, y, use_umax=use_umax)

        print()
        print("=" * 60)
        print("Model Results:")
        if use_umax:
            print(f"  target = {metrics['coef_umax']:.6f}*umax + {metrics['coef_U']:.6f}*U + ({metrics['intercept']:.6f})")
        else:
            print(f"  target = {metrics['coef_U']:.6f}*U + ({metrics['intercept']:.6f})")
        print(f"  R² = {metrics['r2']:.4f}")
        print(f"  MAE = {metrics['mae']:.4f}")
        print("=" * 60)

    if args.cpp_format:
        cpp_output = format_cpp_output(metrics)
        if args.output:
            args.output.write_text(cpp_output + "\n")
            print(f"\nC++ coefficients written to {args.output}")
        else:
            print(f"\nC++ format:\n{cpp_output}")

    return 0


if __name__ == "__main__":
    exit(main())
