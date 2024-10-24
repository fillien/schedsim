#!/usr/bin/env python

import pandas as pd
import subprocess
import os
import sys
import shutil
from concurrent.futures import ThreadPoolExecutor
from io import StringIO
from datetime import datetime

SCHEDVIEW = "./build/schedview/schedview"
PLATFORM = "./platforms/exynos5422LITTLE.json"

def main():
    if len(sys.argv) <= 1:
        print("Please pass the policies as an argument")
        return

    logs = sys.argv[1]

    utilizations = []
    grub = []
    pa = []
    ffa = []
    csf = []

    log_paths = {
        "grub": f"{logs}_logs_grub",
        "pa": f"{logs}_logs_pa",
        "ffa": f"{logs}_logs_ffa",
        "csf": f"{logs}_logs_csf",
    }

    def process_scheduler(scheduler, directory):
        return run_scenario(SCHEDVIEW, os.path.join(log_paths[scheduler], directory))["power"].mean()

    current_util = 0.1

    with ThreadPoolExecutor() as executor:
        for directory in sorted(os.listdir(log_paths["grub"])):
            print(directory)
            utilizations.append(current_util)

            futures = {
                scheduler: executor.submit(process_scheduler, scheduler, directory)
                for scheduler in ["grub", "pa", "ffa", "csf"]
            }

            grub.append(futures["grub"].result())
            pa.append(futures["pa"].result())
            ffa.append(futures["ffa"].result())
            csf.append(futures["csf"].result())

            current_util = round(current_util + 0.1, 2)

    results = pd.DataFrame({
        "util": utilizations,
        "grub": grub,
        "pa": pa,
        "ffa": ffa,
        "csf": csf,
    })

    results.to_csv("data-energy.csv", index=False, sep=" ")
    subprocess.run(["gnuplot", "-e",
                    "output_file='energy_" + logs + ".png';input_file='data-energy.csv'",
                    "./scripts/plot.gp"])


def run_scenario(schedview, logs_path):
    output = subprocess.run(
        [SCHEDVIEW, "--platform", PLATFORM, "--directory", logs_path, "--energy", "--duration"],
        capture_output=True,
        text=True,
        check=True,
    )
    data = StringIO(output.stdout)
    df = pd.read_csv(data, sep=";")
    df["power"] = df["energy"] / df["duration"]
    return df


if __name__ == "__main__":
    main()
