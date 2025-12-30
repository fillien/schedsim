import argparse
import glob
import os
import re
import sys
from collections import defaultdict
from typing import List, Optional, Tuple

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def _parse_utilization_suffix(file_name: str) -> float:
    suffix = os.path.basename(file_name).split("_")[-1].split(".")[0]
    return int(suffix) / 10


def _parse_umax(folder_name: str) -> str:
    base = os.path.basename(folder_name)
    match = re.search(r"umax_(\d+)", base)
    if not match:
        return base
    digits = match.group(1)
    decimals = max(len(digits) - 1, 0)
    divisor = 10 ** decimals if decimals else 1
    value = int(digits) / divisor
    return f"umax = {value:.{decimals}f}"


def _legend_sort_key(label: str) -> float:
    match = re.search(r"=\s*([0-9.]+)", label)
    if match:
        try:
            return float(match.group(1))
        except ValueError:
            return float("inf")
    return float("inf")


def _sanitize_label_for_pgfplots(label: str) -> str:
    sanitized = re.sub(r"[^0-9a-zA-Z]+", "_", label).strip("_")
    if not sanitized:
        sanitized = "series"
    if sanitized[0].isdigit():
        sanitized = f"series_{sanitized}"
    return sanitized


def _collect_folder_data(
    folder_path: str,
) -> Tuple[List[Tuple[float, float, float]], List[Tuple[float, np.ndarray, np.ndarray]]]:
    pattern = os.path.join(folder_path, "ff_cap_search_*.csv")
    csv_paths = sorted(glob.glob(pattern))

    if not csv_paths:
        raise FileNotFoundError(f"No CSV files matched pattern: {pattern}")

    points: List[Tuple[float, float, float]] = []
    curves: List[Tuple[float, np.ndarray, np.ndarray]] = []
    for csv_path in csv_paths:
        df = pd.read_csv(csv_path)
        if "mean_reject_share" not in df.columns or "target" not in df.columns:
            raise KeyError(
                f"Required columns missing in {csv_path}."
            )

        util = _parse_utilization_suffix(csv_path)
        min_idx = df["mean_reject_share"].idxmin()
        min_target = df.loc[min_idx, "target"]
        min_value = df.loc[min_idx, "mean_reject_share"]
        points.append((util, min_target, min_value))
        curves.append(
            (
                util,
                df["target"].to_numpy(dtype=float),
                df["mean_reject_share"].to_numpy(dtype=float),
            )
        )

    points_sorted = sorted(points, key=lambda item: item[0])
    curves_sorted = sorted(curves, key=lambda item: item[0])
    return points_sorted, curves_sorted


def plot_min_targets_for_folders(
    root_path: str,
    export_path: Optional[str],
    regression_export_path: Optional[str],
    pgf_export_path: Optional[str],
    include_folders: Optional[List[str]] = None,
    plot_curves: bool = False,
) -> None:
    folder_pattern = os.path.join(root_path, "min_tasksets_umax_*")
    folder_paths = sorted(
        [path for path in glob.glob(folder_pattern) if os.path.isdir(path)]
    )

    if include_folders:
        include_set = {name.strip() for name in include_folders}
        folder_paths = [
            path for path in folder_paths if os.path.basename(path) in include_set
        ]

    if not folder_paths:
        if include_folders:
            raise FileNotFoundError(
                "No folders matched the provided include list. "
                f"Available folders matching pattern {folder_pattern}."
            )
        raise FileNotFoundError(f"No folders matched pattern: {folder_pattern}")

    plt.figure(figsize=(10, 6))

    cmap = plt.colormaps["tab20"]
    color_values = np.linspace(0, 1, len(folder_paths), endpoint=False)
    colors = [cmap(val) for val in color_values]

    records = []
    regression_records = []
    color_by_label = {}

    for idx, folder in enumerate(folder_paths):
        points, curves = _collect_folder_data(folder)
        utils, targets, values = zip(*points)
        targets_arr = np.array(targets, dtype=float)
        values_arr = np.array(values, dtype=float)

        label = _parse_umax(folder)
        color_by_label[label] = colors[idx]
        if plot_curves:
            for _, curve_targets, curve_values in curves:
                plt.plot(
                    curve_targets,
                    curve_values,
                    color=colors[idx],
                    linewidth=0.9,
                    alpha=0.25,
                    zorder=0,
                )
        plt.scatter(
            targets_arr,
            values_arr,
            label=label,
            color=colors[idx],
            edgecolors="white",
            linewidths=0.6,
            s=60,
            zorder=3,
        )
        plt.plot(
            targets_arr,
            values_arr,
            color=colors[idx],
            linewidth=1.0,
            alpha=0.5,
            zorder=2,
        )
        positive_mask = values_arr > 0
        if positive_mask.sum() >= 2:
            fit_targets = targets_arr[positive_mask]
            fit_values = values_arr[positive_mask]
            slope, intercept = np.polyfit(fit_targets, fit_values, 1)
            if slope != 0:
                x_at_y0 = -intercept / slope
                line_start = min(fit_targets.min(), x_at_y0)
                line_end = max(fit_targets.max(), x_at_y0)
            else:
                x_at_y0 = float("nan")
                line_start = fit_targets.min()
                line_end = fit_targets.max()
            line_x = np.linspace(line_start, line_end, 100)
            line_y = slope * line_x + intercept
            plt.plot(
                line_x,
                line_y,
                color=colors[idx],
                linestyle="--",
                linewidth=1.5,
                alpha=0.8,
                zorder=1,
            )
            regression_records.append(
                {
                    "folder": os.path.basename(folder),
                    "umax_label": label,
                    "regression_points": int(positive_mask.sum()),
                    "min_target_used": float(line_start),
                    "max_target_used": float(line_end),
                    "a": float(slope),
                    "x_at_y0": float(x_at_y0),
                    "color": colors[idx],
                }
            )

        for util, target, value in points:
            records.append(
                {
                    "folder": os.path.basename(folder),
                    "umax_label": label,
                    "utilization": util,
                    "target_for_min_mean_reject_share": target,
                    "min_mean_reject_share": value,
                }
            )

    plt.xlabel("Target")
    plt.ylabel("Minimum Mean Rejected Share")
    plt.title("Minimum Mean Rejected Share vs Target across min_tasksets_umax_* Folders")
    plt.grid(True, linestyle="--", alpha=0.5)
    handles, labels = plt.gca().get_legend_handles_labels()
    if handles:
        order = sorted(range(len(labels)), key=lambda idx: _legend_sort_key(labels[idx]))
        handles = [handles[i] for i in order]
        labels = [labels[i] for i in order]
    plt.legend(handles, labels)
    plt.tight_layout()

    output_path = os.path.join(
        root_path,
        "target_for_min_mean_reject_share_by_umax.png",
    )
    plt.savefig(output_path, dpi=300)
    plt.close()

    if regression_records:
        sorted_regs = sorted(
            regression_records, key=lambda item: _legend_sort_key(item["umax_label"])
        )
        labels = [item["umax_label"] for item in sorted_regs]
        slopes = [item["a"] for item in sorted_regs]
        colors_sorted = [item["color"] for item in sorted_regs]

        plt.figure(figsize=(8, 4.5))
        x_pos = np.arange(len(sorted_regs))
        plt.bar(x_pos, slopes, color=colors_sorted, edgecolor="white", linewidth=0.7)
        plt.xticks(x_pos, labels, rotation=45, ha="right")
        plt.ylabel("Slope (a) in y = a·x + b")
        plt.title("Slope of Minimum Mean Reject Share Regression by umax Folder")
        plt.tight_layout()
        slope_output = os.path.join(
            root_path, "slope_for_min_mean_reject_share_by_umax.png"
        )
        plt.savefig(slope_output, dpi=300)
        plt.close()

    if export_path:
        df = pd.DataFrame.from_records(records)
        df.sort_values(
            by=["umax_label", "utilization"], inplace=True
        )
        df.to_csv(export_path, index=False)
    if regression_export_path and regression_records:
        reg_df = pd.DataFrame.from_records(regression_records)
        reg_df.sort_values(by=["umax_label"], inplace=True)
        if "color" in reg_df.columns:
            reg_df.drop(columns=["color"], inplace=True)
        reg_df.to_csv(regression_export_path, index=False)
    if pgf_export_path:
        label_ids = {}
        used_ids = set()
        sorted_labels = sorted(color_by_label.keys(), key=_legend_sort_key)
        for label in sorted_labels:
            base = _sanitize_label_for_pgfplots(label)
            candidate = base
            suffix = 1
            while candidate in used_ids:
                candidate = f"{base}_{suffix}"
                suffix += 1
            label_ids[label] = candidate
            used_ids.add(candidate)

        counters: defaultdict[str, int] = defaultdict(int)
        pgf_rows = []
        for record in sorted(records, key=lambda item: (_legend_sort_key(item["umax_label"]), item["target_for_min_mean_reject_share"])):
            label = record["umax_label"]
            series_id = label_ids[label]
            point_index = counters[label]
            counters[label] += 1
            pgf_rows.append(
                {
                    "series_id": series_id,
                    "series_label": label,
                    "point_index": point_index,
                    "utilization": record["utilization"],
                    "target": record["target_for_min_mean_reject_share"],
                    "min_mean_reject_share": record["min_mean_reject_share"],
                }
            )

        pgf_df = pd.DataFrame.from_records(pgf_rows)
        pgf_df.sort_values(by=["series_label", "point_index"], inplace=True)
        pgf_df.to_csv(pgf_export_path, index=False)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Plot and export target values yielding minimum mean reject share "
            "for ff_cap_search CSV files grouped by min_tasksets_umax_* folders."
        )
    )
    parser.add_argument("root_path", help="Directory containing min_tasksets_umax_* folders")
    parser.add_argument(
        "--export",
        metavar="CSV_PATH",
        help="Optional path to save the computed minimum target values as CSV",
    )
    parser.add_argument(
        "--regression-export",
        metavar="CSV_PATH",
        help=(
            "Optional path to save slope/intercept parameters for each regression line."
        ),
    )
    parser.add_argument(
        "--pgfplots-export",
        metavar="CSV_PATH",
        help=(
            "Optional path to save data formatted for pgfplots (LaTeX) line/scatter plots."
        ),
    )
    parser.add_argument(
        "--include",
        nargs="+",
        metavar="FOLDER",
        help=(
            "Optional list of folder names (e.g. min_tasksets_umax_01_...) "
            "to include in the plot."
        ),
    )
    parser.add_argument(
        "--show-curves",
        action="store_true",
        help="Plot semi-transparent per-utilization curves behind the minima.",
    )

    args = parser.parse_args()

    export_path = args.export
    if export_path and not os.path.isabs(export_path):
        export_path = os.path.join(args.root_path, export_path)

    regression_export_path = args.regression_export
    if regression_export_path and not os.path.isabs(regression_export_path):
        regression_export_path = os.path.join(args.root_path, regression_export_path)

    pgf_export_path = args.pgfplots_export
    if pgf_export_path and not os.path.isabs(pgf_export_path):
        pgf_export_path = os.path.join(args.root_path, pgf_export_path)

    plot_min_targets_for_folders(
        args.root_path,
        export_path,
        regression_export_path,
        pgf_export_path,
        include_folders=args.include,
        plot_curves=args.show_curves,
    )


if __name__ == "__main__":
    main()
