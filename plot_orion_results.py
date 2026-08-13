#!/usr/bin/env python3
"""Plot the wide CSV files exported by run_orion_benchmarks.py."""

import os
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import polars as pl


DATADIR = Path(os.environ.get(
    "SCHEDSIM_ORION_DATA_DIR",
    "~/Nextcloud/these/manuscript/chap5-dynamic-allocation/"
    "data/orion-o6-unbounded",
)).expanduser()

ALLOCATOR_LABELS = {
    "ff_cap_adaptive_linear": "FFCap Adaptive Linear",
    "ff_cap_u_target_1_0": "FFCap (u_target = 1.0)",
}
POLICY_LABELS = {
    "grub": "GRUB",
    "grub_pa": "GRUB-PA",
    "csf": "CSF",
    "ffa": "FFA",
}
SIG_COLORS = {
    "***": "darkgreen",
    "**": "seagreen",
    "*": "goldenrod",
    "ns": "lightgray",
}


def read_export(filename: str) -> pl.DataFrame:
    """Read one benchmark export from the configured data directory."""
    return pl.read_csv(DATADIR / filename).sort("total_util")


def build_series(labels: dict[str, str]) -> list[tuple[str, str, str, str]]:
    """Build no-migration and migration column/style definitions."""
    series = []
    for column, label in labels.items():
        series.append((column, label, "-", "o"))
        series.append((f"{column}_mig", f"{label} + migration", "--", "s"))
    return series


def add_wide_series(
    ax,
    data: pl.DataFrame,
    series: list[tuple[str, str, str, str]],
    *,
    markersize: float = 5,
) -> None:
    """Plot selected columns from one PGFplots-style wide DataFrame."""
    x_values = data["total_util"].to_numpy()
    for column, label, linestyle, marker in series:
        if column not in data.columns:
            continue
        ax.plot(
            x_values,
            data[column].to_numpy(),
            marker=marker,
            linewidth=1.5,
            markersize=markersize,
            linestyle=linestyle,
            label=label,
        )


def plot_allocator_metric(
    filename: str,
    ylabel: str,
    title: str,
    output: str,
    *,
    ylim: tuple[float, float] | None = None,
) -> None:
    """Plot one allocator metric exported in wide format."""
    fig, ax = plt.subplots(figsize=(10, 5))
    add_wide_series(ax, read_export(filename), build_series(ALLOCATOR_LABELS))
    ax.set_xlabel("Total Utilization")
    ax.set_ylabel(ylabel)
    if ylim is not None:
        ax.set_ylim(*ylim)
    ax.set_title(f"{title}\nOrion O6 — Unbounded tasksets")
    ax.legend()
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output, dpi=150)
    plt.show()


plot_allocator_metric(
    "acceptance.csv",
    "Acceptance Rate",
    "Acceptance Rate — Adaptive vs FFCap (u_target = 1.0)",
    "orion_acceptance_rate.png",
)
plot_allocator_metric(
    "deadline_miss.csv",
    "Deadline Miss Rate",
    "Deadline Miss Rate — Adaptive vs FFCap (u_target = 1.0)",
    "orion_deadline_miss.png",
    ylim=(0, 1),
)
plot_allocator_metric(
    "completion.csv",
    "Effective Completion Rate",
    "Effective Completion Rate — Adaptive vs FFCap (u_target = 1.0)",
    "orion_completion_rate.png",
    ylim=(0, 1),
)


# Per-cluster utilization
fig, axes = plt.subplots(1, 3, figsize=(18, 5))
cluster_exports = [
    ("cluster_util_little.csv", "LITTLE Cluster (A520, perf=0.444)"),
    ("cluster_util_mid.csv", "Mid Cluster (A720, perf=1.0)"),
    ("cluster_util_big.csv", "Big Cluster (A720, perf=1.0)"),
]
for ax, (filename, title) in zip(axes, cluster_exports):
    add_wide_series(
        ax,
        read_export(filename),
        build_series(ALLOCATOR_LABELS),
        markersize=4,
    )
    ax.set_xlabel("Total Utilization")
    ax.set_ylabel("Mean Admitted Utilization")
    ax.set_title(title)
    ax.legend(fontsize=7)
    ax.grid(True, alpha=0.3)
fig.suptitle("Orion O6 — Per-Cluster Admitted Utilization", fontsize=13)
plt.tight_layout()
plt.savefig("orion_cluster_util.png", dpi=150)
plt.show()


# Significance gain, exported separately for each migration setting
fig, axes = plt.subplots(1, 2, figsize=(20, 5), sharey=True)
significance_exports = [
    ("sig_no_mig.csv", "No migration"),
    ("sig_mig.csv", "With migration"),
]
for ax, (filename, title) in zip(axes, significance_exports):
    data = read_export(filename)
    utils = data["total_util"].to_numpy()
    gains = data["mean_gain_pct"].to_numpy()
    labels = data["significance"].to_list()
    colors = [SIG_COLORS[label] for label in labels]
    bars = ax.bar(
        [f"{util:.1f}" for util in utils],
        gains,
        color=colors,
        edgecolor="black",
        linewidth=0.5,
    )
    for bar, label in zip(bars, labels):
        value = bar.get_height()
        offset = 0.15 if value >= 0 else -0.15
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            value + offset,
            label,
            ha="center",
            va="bottom" if value >= 0 else "top",
            fontsize=5,
            fontweight="bold",
        )
    ax.axhline(0, color="black", linewidth=0.8)
    ax.set_xlabel("Total Utilization")
    ax.set_title(f"Acceptance Rate Gain (Adaptive − FFCap)\n{title}")
    ax.tick_params(axis="x", rotation=45, labelsize=6)
    ax.grid(True, axis="y", alpha=0.3)
axes[0].set_ylabel("Mean Acceptance Rate Gain (%)")
axes[1].legend(
    handles=[
        Patch(facecolor="darkgreen", edgecolor="black", label="p < 0.001 (***)"),
        Patch(facecolor="seagreen", edgecolor="black", label="p < 0.01 (**)"),
        Patch(facecolor="goldenrod", edgecolor="black", label="p < 0.05 (*)"),
        Patch(facecolor="lightgray", edgecolor="black", label="not significant (ns)"),
    ],
    fontsize=8,
    loc="upper left",
)
fig.suptitle("Orion O6 — Acceptance Rate Gain (Adaptive − FFCap)", fontsize=13)
plt.tight_layout()
plt.savefig("orion_sig_gain.png", dpi=150)
plt.show()


# Migration count
fig, ax = plt.subplots(figsize=(10, 5))
add_wide_series(
    ax,
    read_export("migration_count.csv"),
    [(column, label, "-", "o") for column, label in ALLOCATOR_LABELS.items()],
)
ax.set_xlabel("Total Utilization")
ax.set_ylabel("Mean Cluster Migrations")
ax.set_title("Inter-Cluster Migrations — Orion O6 (migration enabled)")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig("orion_migration_count.png", dpi=150)
plt.show()


# Energy benchmark
fig, axes = plt.subplots(1, 4, figsize=(22, 5))
energy_exports = [
    ("energy_power.csv", "Mean Average Power (mW)", "Average Power Consumption", None),
    ("energy_acceptance.csv", "Acceptance Rate", "Acceptance Rate", None),
    ("energy_deadline_miss.csv", "Deadline Miss Rate", "Deadline Miss Rate", (0, 1)),
    ("energy_completion.csv", "Effective Completion Rate", "Effective Completion Rate", (0, 1)),
]
for ax, (filename, ylabel, title, ylim) in zip(axes, energy_exports):
    add_wide_series(ax, read_export(filename), build_series(POLICY_LABELS), markersize=4)
    ax.set_xlabel("Total Utilization")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend(fontsize=7)
    ax.grid(True, alpha=0.3)
    if ylim is not None:
        ax.set_ylim(*ylim)
fig.suptitle(
    "Orion O6 — Energy Benchmark (unbounded tasksets, migration comparison)",
    fontsize=13,
)
plt.tight_layout()
plt.savefig("orion_energy_benchmark.png", dpi=150)
plt.show()

print(f"All plots saved from CSV files in {DATADIR}.")
