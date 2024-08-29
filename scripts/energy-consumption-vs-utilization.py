#!/usr/bin/env python

import pandas as pd
import subprocess
import os
import sys
import shutil
import concurrent.futures
from datetime import datetime

SCHEDSIM = "./build/schedsim/schedsim"
SCHEDGEN = "./build/schedgen/schedgen"
SCHEDVIEW = "./build/schedview/schedview"

def main():
    if len(sys.argv) <= 1:
        print("Please pass the policies as an argument")
        return

    policies = sys.argv[1].split(',')

    # Create a directory to store taskset
    datadir = f"data_{datetime.now().strftime('%Y-%m-%d-%H-%M-%S')}"

    sys.stdout.write("Generate tasksets...")
    sys.stdout.flush()
    generate_tasksets(datadir, nb_taskset=15, nb_task=10, nb_jobs=100)
    print("OK")

    results = pd.DataFrame()
    first = True
    
    for policy in policies:
        print("Simulate with " + policy + "...")
        if first:
            results = simulate(datadir, policy)
            first = False
        else:
            results_simu = simulate(datadir, policy)
            results = merge(results, results_simu)

        print("OK")

    print(results)

    results.to_csv("data.csv", index=False, sep=" ")
    # subprocess.run(["gnuplot", "./script/plot.gp"])

def generate_tasksets(datadir, nb_taskset, nb_task, nb_jobs):
    if os.path.exists(datadir):
        shutil.rmtree(datadir)
    os.mkdir(datadir)

    for i in range(1, 31):
        utilization = i / 10.0
        name = str(utilization).replace(".", "_")
        current_sce = os.path.join(datadir, f"util_{name}")
        os.mkdir(current_sce)

        for j in range(1, nb_taskset + 1):
            subprocess.run(
                [
                    SCHEDGEN,
                    "taskset",
                    "-t",
                    str(nb_task),
                    "-j",
                    str(nb_jobs),
                    "-s",
                    "1",
                    "-u",
                    str(utilization),
                    "-o",
                    os.path.join(current_sce, f"{j}.json"),
                ]
            )

def run_scenario(schedsim, schedview, current_dir, scenario, sched_policy, datadir):
    logs_path = os.path.join(datadir, scenario + "logs.json")
    subprocess.run(
        [
            schedsim,
            "-s",
            os.path.join(current_dir, scenario),
            "-p",
            str(sched_policy),
            "-o",
            logs_path,
        ],
        check=True,
    )
    result = subprocess.run(
        [schedview, logs_path, "--cli", "--energy"],
        capture_output=True,
        text=True,
    )
    return float(result.stdout.strip())

def simulate(datadir, sched_policy):
    utilization_results = []
    energy_results = []
    min_results = []
    max_results = []
    data_table = {}
    utilisation = 0

    for directory in sorted(os.listdir(datadir)):
        current_dir = os.path.join(datadir, directory)
        if os.path.isdir(current_dir):
            print(current_dir)
            count = 0
            sum_energy = 0
            utilisation = round(utilisation + 0.1, 2)
            max_energy = 0
            min_energy = 0

            scenarios = sorted(os.listdir(current_dir))
            with concurrent.futures.ThreadPoolExecutor() as executor:
                futures = [executor.submit(run_scenario, SCHEDSIM, SCHEDVIEW, current_dir, scenario, sched_policy, datadir) for scenario in scenarios]
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


def merge(df1, df2):
    return pd.merge(df1, df2, on="utilization")

if __name__ == "__main__":
    main()
