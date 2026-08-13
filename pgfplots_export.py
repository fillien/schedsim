"""Helpers to pivot simulation DataFrames into PGFplots-friendly wide CSVs.

PGFplots reads CSVs best in wide format:
    total_util, adaptive, adaptive_mig, baseline, baseline_mig
so each \addplot reads one column with no row filtering needed.
"""
import os
import polars as pl


def _pivot_alloc(df: pl.DataFrame, value_col: str, has_migration: bool) -> pl.DataFrame:
    """Pivot allocator DataFrame from long to wide format."""
    if has_migration:
        df = df.with_columns(
            pl.when(pl.col("migration"))
            .then(pl.col("allocator") + "_mig")
            .otherwise(pl.col("allocator"))
            .alias("series")
        )
    else:
        df = df.with_columns(pl.col("allocator").alias("series"))

    return (
        df.select(["total_util", "series", value_col])
        .pivot(on="series", index="total_util", values=value_col)
        .sort("total_util")
    )


def _pivot_energy(df: pl.DataFrame, value_col: str, has_migration: bool) -> pl.DataFrame:
    """Pivot energy DataFrame from long to wide format."""
    if has_migration:
        df = df.with_columns(
            pl.when(pl.col("migration"))
            .then(pl.col("policy") + "_mig")
            .otherwise(pl.col("policy"))
            .alias("series")
        )
    else:
        df = df.with_columns(pl.col("policy").alias("series"))

    return (
        df.select(["total_util", "series", value_col])
        .pivot(on="series", index="total_util", values=value_col)
        .sort("total_util")
    )


def export_alloc_csvs(
    comparison_df: pl.DataFrame,
    cluster_summary: pl.DataFrame,
    sig_df: pl.DataFrame,
    outdir: str,
    has_migration: bool = False,
    migration_summary: pl.DataFrame | None = None,
):
    """Export allocator comparison results as PGFplots-ready wide CSVs."""
    os.makedirs(outdir, exist_ok=True)
    files = []

    # Acceptance, deadline miss, completion — one CSV each, wide format
    for metric, col_name in [
        ("acceptance", "acceptance_rate"),
        ("deadline_miss", "deadline_miss_rate"),
        ("completion", "completion_rate"),
    ]:
        if col_name not in comparison_df.columns:
            # Compute on the fly
            if col_name == "acceptance_rate":
                agg_df = comparison_df.group_by(["allocator"] + (["migration"] if has_migration else []) + ["total_util"]).agg(
                    acceptance_rate=1.0 - pl.col("rejected_arrivals").sum() / pl.col("total_arrivals").sum()
                )
            elif col_name == "deadline_miss_rate":
                agg_df = comparison_df.group_by(["allocator"] + (["migration"] if has_migration else []) + ["total_util"]).agg(
                    deadline_miss_rate=pl.col("deadline_misses").sum() / pl.col("total_jobs").sum()
                )
            elif col_name == "completion_rate":
                agg_df = comparison_df.group_by(["allocator"] + (["migration"] if has_migration else []) + ["total_util"]).agg(
                    completion_rate=pl.col("completed_jobs").sum() / pl.col("total_arrivals").sum()
                )
        else:
            agg_df = comparison_df

        wide = _pivot_alloc(agg_df, col_name, has_migration)
        path = os.path.join(outdir, f"{metric}.csv")
        wide.write_csv(path)
        files.append((path, wide.shape))

    # Per-cluster utilization — one CSV per cluster
    if cluster_summary is not None:
        for col, name in [
            ("mean_little_util", "cluster_util_little"),
            ("mean_big_util", "cluster_util_big"),
            ("mean_mid_util", "cluster_util_mid"),
        ]:
            if col not in cluster_summary.columns:
                continue
            wide = _pivot_alloc(cluster_summary, col, has_migration)
            path = os.path.join(outdir, f"{name}.csv")
            wide.write_csv(path)
            files.append((path, wide.shape))

    # Significance — one CSV per migration state (or single if no migration)
    if has_migration and "migration" in sig_df.columns:
        for mig_val, suffix in [(False, "no_mig"), (True, "mig")]:
            sub = sig_df.filter(pl.col("migration") == mig_val).drop("migration").sort("total_util")
            path = os.path.join(outdir, f"sig_{suffix}.csv")
            sub.write_csv(path)
            files.append((path, sub.shape))
    else:
        path = os.path.join(outdir, "sig.csv")
        sig_df.sort("total_util").write_csv(path)
        files.append((path, sig_df.shape))

    # Migration count — wide format (only if migration enabled)
    if migration_summary is not None:
        wide = migration_summary.pivot(
            on="allocator", index="total_util", values="mean_migrations"
        ).sort("total_util")
        path = os.path.join(outdir, "migration_count.csv")
        wide.write_csv(path)
        files.append((path, wide.shape))

    for path, shape in files:
        print(f"  {os.path.basename(path)}: {shape[0]} rows × {shape[1]} cols")
    return files


def export_energy_csvs(
    energy_summary: pl.DataFrame,
    outdir: str,
    has_migration: bool = False,
):
    """Export energy benchmark results as PGFplots-ready wide CSVs."""
    os.makedirs(outdir, exist_ok=True)
    files = []

    for metric, col_name in [
        ("energy_power", "mean_avg_power_mw"),
        ("energy_acceptance", "acceptance_rate"),
        ("energy_deadline_miss", "deadline_miss_rate"),
        ("energy_completion", "completion_rate"),
    ]:
        if col_name not in energy_summary.columns:
            continue
        wide = _pivot_energy(energy_summary, col_name, has_migration)
        path = os.path.join(outdir, f"{metric}.csv")
        wide.write_csv(path)
        files.append((path, wide.shape))

    for path, shape in files:
        print(f"  {os.path.basename(path)}: {shape[0]} rows × {shape[1]} cols")
    return files
