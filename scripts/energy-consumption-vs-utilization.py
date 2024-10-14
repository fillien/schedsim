#!/usr/bin/env python

import pandas as pd
import subprocess
import os
import sys
import shutil
import concurrent.futures
from datetime import datetime

SCHEDVIEW = "./build/schedview/schedview"
PLATFORM = "./platforms/exynos5422LITTLE.json"
LOG_PATH = "logs.json"

def main():
    if len(sys.argv) <= 1:
        print("Please pass the policies as an argument")
        return

    logs = sys.argv[1]

    log_paths = {}
    log_paths["grub"] = f"{logs}_logs_grub"
    log_paths["pa"] = f"{logs}_logs_pa"
    log_paths["ffa"] = f"{logs}_logs_ffa"
    log_paths["csf"] = f"{logs}_logs_csf"

    results = pd.DataFrame()
    first = True

    for path in log_paths.items():
        print(path[0])
        if first:
            results = simulate(path[1], path[0])
            first = False
        else:
            results_simu = simulate(path[1], path[0])
            results = merge(results, results_simu)

    results.to_csv("data-energy.csv", index=False, sep=" ")
    # subprocess.run(["gnuplot", "./scripts/plot.gp"])


def simulate(logsdir, sched_policy):
    util_paths = sorted(os.listdir(logsdir))

    utilization_results = []
    energy_results = []
    min_results = []
    max_results = []
    data_table = {}
    utilisation = 0

    for directory in sorted(os.listdir(logsdir)):
        current_dir = os.path.join(logsdir, directory)
        if not os.path.isdir(current_dir):
            print("error: " + current_dir + " is not a directory")
            sys.exit(1)

        count = 0
        sum_energy = 0
        utilisation = round(utilisation + 0.1, 2)
        max_energy = 0
        min_energy = 0

        scenarios = sorted(os.listdir(current_dir))
        with concurrent.futures.ThreadPoolExecutor() as executor:
            futures = [
                executor.submit(
                    run_scenario, SCHEDVIEW, os.path.join(current_dir, scenario)
                )
                for scenario in scenarios
            ]
            for future in concurrent.futures.as_completed(futures):
                count += 1
                energy = future.result()
                if count == 1:
                    min_energy = energy
                    max_energy = energy
                else:
                    min_energy = min(energy, min_energy)
                    max_energy = max(energy, max_energy)
                sum_energy += energy

        energy_results.append(round(sum_energy / count, 3))
        min_results.append(min_energy)
        max_results.append(max_energy)
        utilization_results.append(utilisation)

    data_table["utilization"] = utilization_results
    data_table[str(sched_policy)] = energy_results
    data_table[str(sched_policy) + "_min"] = min_results
    data_table[str(sched_policy) + "_max"] = max_results
    return pd.DataFrame(data_table)


def run_scenario(schedview, log_path):
    output = subprocess.run(
        [SCHEDVIEW, "--platform", PLATFORM, log_path, "--energy", "--duration"],
        capture_output=True,
        text=True,
        check=True,
    )
    df = parse_output(output)
    return df.loc[0, "energy"] / df.loc[0, "duration"]


def parse_output(output):
    lines = output.stdout.strip().split("\n")
    headers = lines[0].split()
    data_lines = lines[1:]
    parsed_output = [
        {headers[i]: float(values[i]) for i in range(len(headers))}
        for values in (line.split() for line in data_lines)
    ]
    return pd.DataFrame(parsed_output)


def merge(df1, df2):
    return pd.merge(df1, df2, on="utilization")


if __name__ == "__main__":
    main()
