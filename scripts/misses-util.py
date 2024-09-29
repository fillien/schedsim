#!/usr/bin/env python

import pandas as pd
import subprocess
import os
import sys
import shutil
import concurrent.futures
from datetime import datetime

SCHEDVIEW = "./build/schedview/schedview"


def process_scenario(util, log_paths, directory):
    grub_dir = os.path.join(log_paths["grub"], directory)
    print(grub_dir)

    sce_stats = []

    scenarios = sorted(os.listdir(grub_dir))

    with concurrent.futures.ThreadPoolExecutor() as executor:
        futures = []
        for scenario in scenarios:
            grub_sce = os.path.join(log_paths["grub"], directory, scenario)
            pa_sce = os.path.join(log_paths["pa"], directory, scenario)
            pa_f_min_sce = os.path.join(log_paths["pa_f_min"], directory, scenario)
            pa_m_min_sce = os.path.join(log_paths["pa_m_min"], directory, scenario)

            # Schedule the scenario processing in parallel
            futures.append(executor.submit(run_scenario, SCHEDVIEW, grub_sce))
            futures.append(executor.submit(run_scenario, SCHEDVIEW, pa_sce))
            futures.append(executor.submit(run_scenario, SCHEDVIEW, pa_f_min_sce))
            futures.append(executor.submit(run_scenario, SCHEDVIEW, pa_m_min_sce))

        # Collect the results from futures
        raw_results = [future.result() for future in futures]
        raw_results = compare_list(raw_results, 1)
        normalized_results = normalize_list(raw_results)

        sce_stats.append(
            {
                "utilization": util,
                "grub": normalized_results[0],
                "pa": normalized_results[1],
                "pa_f_min": normalized_results[2],
                "pa_m_min": normalized_results[3],
            }
        )

    return pd.DataFrame(sce_stats)


def main():
    if len(sys.argv) <= 1:
        print("Please pass the policies as an argument")
        return

    logs = sys.argv[1]

    log_paths = {
        "grub": f"{logs}_logs_grub",
        "pa": f"{logs}_logs_pa",
        "pa_f_min": f"{logs}_logs_pa_f_min",
        "pa_m_min": f"{logs}_logs_pa_m_min",
    }

    results = []
    util = 0

    # Parallelize the processing of each directory
    directories = sorted(os.listdir(log_paths["grub"]))

    with concurrent.futures.ThreadPoolExecutor() as executor:
        futures = []
        for directory in directories:
            util = round(util + 0.1, 2)
            futures.append(
                executor.submit(process_scenario, util, log_paths, directory)
            )

        for future in concurrent.futures.as_completed(futures):
            sce_stats_df = future.result()
            results.append(
                {
                    "utilization": sce_stats_df["utilization"].mean(),
                    "grub": sce_stats_df["grub"].mean(),
                    "pa": sce_stats_df["pa"].mean(),
                    "pa_f_min": sce_stats_df["pa_f_min"].mean(),
                    "pa_m_min": sce_stats_df["pa_m_min"].mean(),
                }
            )

    output = pd.DataFrame(results)
    output = output.sort_values(by=["utilization"])
    print(output)

    output.to_csv("data-misses.csv", index=False, sep=" ")
    subprocess.run(["gnuplot", "./scripts/plot-misses.gp"])


def compare_list(data, ref_index):
    reference = data[ref_index]
    return [(x - reference) for x in data]


def normalize_list(data):
    min_val = min(data)
    max_val = max(data)
    if (max_val - min_val) == 0:
        return [0.0 for x in data]
    return [(x - min_val) / (max_val - min_val) for x in data]


def run_scenario(schedview, log_path):
    result = subprocess.run(
        [schedview, log_path, "--deadlines-rates"],
        capture_output=True,
        text=True,
        check=True,
    )
    return float(result.stdout.strip())


if __name__ == "__main__":
    main()
