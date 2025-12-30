import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import glob
import os

def plot_reject_share(folder_path):
    plt.figure(figsize=(10, 6))

    file_paths = sorted(glob.glob(f"{folder_path}/ff_cap_search_*.csv"))


    cmap = plt.colormaps.get_cmap("tab20")
    color_values = np.linspace(0, 1, len(file_paths), endpoint=False)
    colors = [cmap(val) for val in color_values]

    added_min_marker = False

    for idx, file in enumerate(file_paths):
        df = pd.read_csv(file)
        suffix = os.path.basename(file).split("_")[-1].split(".")[0]  # e.g. "39"
        utilization = int(suffix) / 10
        plt.plot(
            df["target"],
            df["mean_reject_share"],
            label=f"U = {utilization:.1f}",
            color=colors[idx],
        )
        min_idx = df["mean_reject_share"].idxmin()
        min_target = df.loc[min_idx, "target"]
        min_value = df.loc[min_idx, "mean_reject_share"]
        plt.scatter(
            [min_target],
            [min_value],
            color="#d62728",
            edgecolors="white",
            linewidths=0.6,
            s=60,
            zorder=3,
            label="Minimum mean reject share" if not added_min_marker else None,
        )
        if not added_min_marker:
            added_min_marker = True

    plt.xlabel("Utilization cap")
    plt.ylabel("Mean Rejected Share")
    plt.title("Mean Rejected Share by Utilization cap across Utilizations")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()

    output_path = os.path.join(
        folder_path,
        "mean_reject_share_vs_target_by_utilization.png",
    )
    plt.savefig(output_path, dpi=300)
    plt.close()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python plot_reject_share.py <folder_path>")
        sys.exit(1)
    folder_path = sys.argv[1]
    plot_reject_share(folder_path)
