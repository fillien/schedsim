import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import glob
import os

def plot_acceptance_rate(folder_path):
    plt.figure(figsize=(10, 6))

    file_paths = sorted(glob.glob(f"{folder_path}/ff_cap_search_*.csv"))

    cmap = plt.colormaps.get_cmap("tab20")
    color_values = np.linspace(0, 1, len(file_paths), endpoint=False)
    colors = [cmap(val) for val in color_values]

    added_max_marker = False

    for idx, file in enumerate(file_paths):
        df = pd.read_csv(file)
        suffix = os.path.basename(file).split("_")[-1].split(".")[0]  # e.g. "39"
        utilization = int(suffix) / 10
        acceptance_rate = 1 - df["mean_reject_share"]
        plt.plot(
            df["target"],
            acceptance_rate,
            label=f"U = {utilization:.1f}",
            color=colors[idx],
        )
        max_idx = acceptance_rate.idxmax()
        max_target = df.loc[max_idx, "target"]
        max_value = acceptance_rate.loc[max_idx]
        plt.scatter(
            [max_target],
            [max_value],
            color="#2ca02c",
            edgecolors="white",
            linewidths=0.6,
            s=60,
            zorder=3,
            label="Maximum acceptance rate" if not added_max_marker else None,
        )
        if not added_max_marker:
            added_max_marker = True

    plt.xlabel("Utilization cap")
    plt.ylabel("Acceptance Rate")
    plt.title("Acceptance Rate by Utilization Cap across Utilizations")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()

    output_path = os.path.join(
        folder_path,
        "acceptance_rate_vs_target_by_utilization.png",
    )
    plt.savefig(output_path, dpi=300)
    plt.close()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python plot_acceptance_rate.py <folder_path>")
        sys.exit(1)
    folder_path = sys.argv[1]
    plot_acceptance_rate(folder_path)
