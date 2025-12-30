#!/usr/bin/env python3
"""Analyze allocation results by utilization using a counting baseline for normalization."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import polars as pl

pl.Config.set_tbl_rows(-1)
pl.Config.set_tbl_cols(-1)
# pl.Config.set_tbl_width_chars(200)


DEFAULT_INPUT = Path("min_taskset_data.csv")
DEFAULT_OUTPUT = Path("utilization_reject_share.png")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        "-i",
        type=Path,
        default=DEFAULT_INPUT,
        help="Path to the aggregated min_taskset data CSV (semicolon separated).",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="Path to save the reject-share plot (PNG).",
    )
    parser.add_argument(
        "--scatter-output",
        type=Path,
        default=None,
        help="Optional path to save a per-taskset scatter plot comparing algorithms to ff_little_first.",
    )
    parser.add_argument(
        "--stat",
        choices=("mean", "median"),
        default="mean",
        help="Which statistic to use when aggregating reject shares by utilization.",
    )
    parser.add_argument(
        "--gain-output",
        type=Path,
        default=None,
        help="Optional path to save an aggregated bar chart of gain vs ff_little_first with 95% CI.",
    )
    return parser.parse_args()


def load_data(csv_path: Path) -> pl.DataFrame:
    if not csv_path.exists():
        raise FileNotFoundError(f"Input CSV not found: {csv_path}")

    df = pl.read_csv(csv_path, separator=";", try_parse_dates=False)

    required_columns = {"tasksets", "counting", "ff_little_first"}
    missing = required_columns - set(df.columns)
    if missing:
        missing_list = ", ".join(sorted(missing))
        raise ValueError(f"Input CSV is missing required column(s): {missing_list}")

    df = df.with_columns(
        pl.col("tasksets")
        .str.extract(r"/(\d+)/")
        .alias("util_folder")
    ).with_columns(
        (pl.col("util_folder").cast(pl.Float64) / 10.0).alias("total_utilization")
    )

    if df["total_utilization"].null_count() > 0:
        raise ValueError("Failed to parse utilization for all tasksets. Check path format.")

    return df.drop("util_folder")


def compute_reject_share(df: pl.DataFrame) -> pl.DataFrame:
    value_cols = [
        c for c in df.columns if c not in {"tasksets", "total_utilization", "counting"}
    ]
    if not value_cols:
        raise ValueError("No algorithm result columns found to compare against counting baseline.")

    long_df = df.unpivot(
        index=["tasksets", "total_utilization", "counting"],
        on=value_cols,
        variable_name="algorithm",
        value_name="rejections",
    )

    # Ensure numeric types for arithmetic.
    long_df = long_df.with_columns(
        [
            pl.col("rejections").cast(pl.Float64, strict=False),
            pl.col("counting").cast(pl.Float64, strict=False),

        ]
    )

    # Normalize rejections by the counting allocator's allocation volume.
    long_df = long_df.filter(pl.col("counting").is_not_null())

    reject_share = (
        pl.when(pl.col("counting") > 0)
        .then(pl.col("rejections") / pl.col("counting"))
        .otherwise(0.0)
        .cast(pl.Float64)
        .alias("reject_share")
    )

    long_df = long_df.with_columns([reject_share])

    baseline_df = (
        long_df.filter(pl.col("algorithm") == "ff_little_first")
        .select(["tasksets", "total_utilization", "reject_share"])
        .rename({"reject_share": "baseline_share"})
    )

    long_df = long_df.join(baseline_df, on=["tasksets", "total_utilization"], how="left")

    long_df = long_df.with_columns(
        (pl.col("baseline_share") - pl.col("reject_share")).alias("gain_vs_ff_little_first")
    )
    return long_df


def aggregate_by_utilization(long_df: pl.DataFrame, stat: str) -> pl.DataFrame:
    if long_df.is_empty():
        return pl.DataFrame(
            {
                "total_utilization": [],
                "algorithm": [],
                "reject_share": [],
                "gain_vs_ff_little_first": [],
                "baseline_share": [],
                "n": [],
                "std": [],
                "ci95": [],
                "gain_std": [],
                "gain_ci95": [],
            }
        )

    if stat == "median":
        share_expr = pl.col("reject_share").median().alias("reject_share")
        gain_expr = (
            pl.col("gain_vs_ff_little_first").median().alias("gain_vs_ff_little_first")
        )
        baseline_expr = pl.col("baseline_share").median().alias("baseline_share")
    else:
        share_expr = pl.col("reject_share").mean().alias("reject_share")
        gain_expr = pl.col("gain_vs_ff_little_first").mean().alias("gain_vs_ff_little_first")
        baseline_expr = pl.col("baseline_share").mean().alias("baseline_share")

    aggregated = (
        long_df.group_by(["total_utilization", "algorithm"])
        .agg([
            share_expr,
            gain_expr,
            baseline_expr,
            pl.len().alias("n"),
            pl.col("reject_share").std().alias("std"),
            pl.col("gain_vs_ff_little_first").std().alias("gain_std"),
        ])
        .sort(["total_utilization", "algorithm"])
    )
    aggregated = aggregated.with_columns(
        [
            (pl.lit(1.96) * pl.col("std") / pl.col("n").sqrt()).alias("ci95"),
            (pl.lit(1.96) * pl.col("gain_std") / pl.col("n").sqrt()).alias("gain_ci95"),
        ]
    )

    return aggregated


def plot_taskset_gain(long_df: pl.DataFrame, output_path: Path) -> None:
    if output_path is None:
        return

    subset = long_df.filter(pl.col("algorithm") != "ff_little_first")
    if subset.is_empty():
        raise ValueError("No algorithms other than ff_little_first available for gain plot.")

    plt.figure(figsize=(10, 6))
    for alg_df in subset.partition_by("algorithm", maintain_order=True):
        algorithm = alg_df["algorithm"][0]
        plt.scatter(
            alg_df["total_utilization"].to_numpy(),
            alg_df["gain_vs_ff_little_first"].to_numpy(),
            label=algorithm,
            alpha=0.7,
            s=30,
        )

    plt.axhline(0.0, color="gray", linestyle="--", linewidth=1)
    plt.xlabel("Total utilization")
    plt.ylabel("Gain vs ff_little_first (positive = fewer rejections)")
    plt.title("Per-taskset gain/loss relative to ff_little_first")
    plt.legend(loc="best")
    plt.tight_layout()
    plt.savefig(output_path, dpi=300)
    plt.close()


def plot_aggregated(aggregated: pl.DataFrame, output_path: Path) -> None:
    if aggregated.is_empty():
        raise ValueError("Aggregated data is empty; nothing to plot.")

    # Filter to start plotting at total_utilization >= 3.5
    filtered = aggregated.filter(pl.col("total_utilization") >= 3.9)
    if filtered.is_empty():
        raise ValueError("No data at or above total_utilization 3.5 to plot.")
    pivoted = (
        filtered.pivot(
            values="reject_share",
            index="total_utilization",
            on="algorithm",
        )
        .sort("total_utilization")
    )
    ci_pivoted = (
        filtered.pivot(
            values="ci95",
            index="total_utilization",
            on="algorithm",
        )
        .sort("total_utilization")
    )

    pdf = pivoted.to_pandas()
    ci_pdf = ci_pivoted.to_pandas()
    util_values = pdf.pop("total_utilization")
    ci_pdf.pop("total_utilization")

    fig, ax = plt.subplots(figsize=(10, 6), constrained_layout=True)
    x = np.arange(len(util_values))
    algorithms = list(pdf.columns)
    n_alg = len(algorithms)
    if n_alg == 0:
        raise ValueError("No algorithm columns found after pivot; nothing to plot.")
    bar_width = 0.8 / max(n_alg, 1)

    for i, algorithm in enumerate(algorithms):
        series = pdf[algorithm]
        yerr = ci_pdf[algorithm].to_numpy() if algorithm in ci_pdf.columns else None
        ax.bar(x + i * bar_width, series, width=bar_width, label=algorithm, yerr=yerr, error_kw={"capsize": 3})

    ax.set_xticks(x + bar_width * (n_alg - 1) / 2.0)
    ax.set_xticklabels([f"{u:.1f}" for u in util_values])

    ax.set_xlabel("Total Utilization")
    ax.set_ylabel("Reject share (rejections / counting allocations)")
    ax.set_title("Allocation Reject Share by Utilization")
    ax.grid(True, linestyle="--", alpha=0.4)
    ax.legend(title="Algorithm", loc="best")

    fig.savefig(output_path, dpi=300)
    plt.close(fig)
    print(aggregated)

def plot_aggregated_gain(aggregated: pl.DataFrame, output_path: Path) -> None:
    if output_path is None:
        return

    if aggregated.is_empty():
        raise ValueError("Aggregated data is empty; nothing to plot.")

    # Filter to start plotting at total_utilization > 3.7
    filtered = aggregated.filter(pl.col("total_utilization") > 3.9)
    if filtered.is_empty():
        raise ValueError("No data above total_utilization 3.7 to plot.")

    pivoted = (
        filtered.pivot(
            values="gain_vs_ff_little_first",
            index="total_utilization",
            on="algorithm",
        )
        .sort("total_utilization")
    )
    ci_pivoted = (
        filtered.pivot(
            values="gain_ci95",
            index="total_utilization",
            on="algorithm",
        )
        .sort("total_utilization")
    )

    pdf = pivoted.to_pandas()
    ci_pdf = ci_pivoted.to_pandas()
    util_values = pdf.pop("total_utilization")
    ci_pdf.pop("total_utilization")

    fig, ax = plt.subplots(figsize=(10, 6), constrained_layout=True)
    x = np.arange(len(util_values))
    algorithms = list(pdf.columns)
    n_alg = len(algorithms)
    if n_alg == 0:
        raise ValueError("No algorithm columns found after pivot; nothing to plot.")
    bar_width = 0.8 / max(n_alg, 1)

    for i, algorithm in enumerate(algorithms):
        series = pdf[algorithm]
        yerr = ci_pdf[algorithm].to_numpy() if algorithm in ci_pdf.columns else None
        ax.bar(
            x + i * bar_width,
            series,
            width=bar_width,
            label=algorithm,
            yerr=yerr,
            error_kw={"capsize": 3},
        )

    ax.set_xticks(x + bar_width * (n_alg - 1) / 2.0)
    ax.set_xticklabels([f"{u:.1f}" for u in util_values])

    ax.set_xlabel("Total Utilization")
    ax.set_ylabel("Gain vs ff_little_first (positive = fewer rejections)")
    ax.set_title("Aggregated gain vs ff_little_first by Utilization")
    ax.grid(True, linestyle="--", alpha=0.4)
    ax.legend(title="Algorithm", loc="best")

    fig.savefig(output_path, dpi=300)
    plt.close(fig)


def main() -> None:
    args = parse_args()
    df = load_data(args.input)
    print(df)
    long_df = compute_reject_share(df)
    aggregated = aggregate_by_utilization(long_df, args.stat)

    print("Per-taskset reject share (first 10 rows):")
    print(long_df.head(10))
    print("\nAggregated reject share by utilization:")
    print(aggregated)

    plot_aggregated(aggregated, args.output)
    print(f"Plot saved to {args.output}")
    if args.gain_output is not None:
        plot_aggregated_gain(aggregated, args.gain_output)
        print(f"Aggregated gain plot saved to {args.gain_output}")

    if args.scatter_output is not None:
        plot_taskset_gain(long_df, args.scatter_output)
        print(f"Gain plot saved to {args.scatter_output}")


if __name__ == "__main__":
    main()
