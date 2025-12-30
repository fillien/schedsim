import glob
import os
import sys

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def plot_minimum_reject_share(folder_path: str) -> None:
    pattern = os.path.join(folder_path, "ff_cap_search_*.csv")
    file_paths = sorted(glob.glob(pattern))

    if not file_paths:
        raise FileNotFoundError(f"No files matched pattern: {pattern}")

    cmap = plt.colormaps["tab20"]
    color_values = np.linspace(0, 1, len(file_paths), endpoint=False)
    colors = [cmap(val) for val in color_values]

    utilizations = []
    min_targets = []

    for file_path in file_paths:
        df = pd.read_csv(file_path)

        if "mean_reject_share" not in df.columns:
            raise KeyError(
                f"Column 'mean_reject_share' not found in {file_path}."
            )

        suffix = os.path.basename(file_path).split("_")[-1].split(".")[0]
        utilization = int(suffix) / 10

        utilizations.append(utilization)
        min_index = df["mean_reject_share"].idxmin()
        min_target = df.loc[min_index, "target"]

        min_targets.append(min_target)

    plt.figure(figsize=(10, 6))
    plt.scatter(utilizations, min_targets, c=colors, s=80)
    plt.plot(utilizations, min_targets, color="gray", linewidth=1, alpha=0.5)

    plt.xlabel("Dataset Utilization")
    plt.ylabel("Target for Minimum Mean Rejected Share")
    plt.title("Target Achieving Minimum Mean Rejected Share vs Utilization")
    plt.xticks(utilizations, [f"U = {u:.1f}" for u in utilizations], rotation=45, ha="right")
    plt.tight_layout()
    plt.grid(True, linestyle="--", alpha=0.5)

    output_path = os.path.join(
        folder_path,
        "target_for_min_mean_reject_share_vs_utilization.png",
    )
    plt.savefig(output_path, dpi=300)
    plt.close()


def main() -> None:
    if len(sys.argv) != 2:
        print("Usage: python plot_min_reject_share.py <folder_path>")
        sys.exit(1)

    folder = sys.argv[1]
    plot_minimum_reject_share(folder)


if __name__ == "__main__":
    main()
