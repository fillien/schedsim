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

def run_scenario(schedview, logs_path):
    output = subprocess.run(
        [SCHEDVIEW, "--platform", PLATFORM, "--directory", logs_path, "--index", "--energy", "--duration"],
        capture_output=True,
        text=True,
        check=True,
    )
    data = StringIO(output.stdout)
    df = pd.read_csv(data, sep=";")
    df["power"] = df["energy"] / df["duration"]
    return df


def process_scheduler(scheduler, directory, log_paths):
    """Process a scheduler's log directory to calculate mean power."""
    return run_scenario(SCHEDVIEW, os.path.join(log_paths[scheduler], directory))["power"].mean()


def process_logs(logs, suffix, output_csv, output_img):
    """Generalized function to process logs for a given test type."""
    utilizations, pa, ffa, csf = [], [], [], []
    log_paths = {
        "pa": f"{logs}_logs_pa_{suffix[0]}",
        "ffa": f"{logs}_logs_ffa_{suffix[1]}",
        "csf": f"{logs}_logs_csf_{suffix[2]}",
    }

    current_util = 0.1

    with ThreadPoolExecutor() as executor:
        for directory in sorted(os.listdir(log_paths["pa"])):
            print(directory)
            utilizations.append(current_util)

            futures = {
                scheduler: executor.submit(process_scheduler, scheduler, directory, log_paths)
                for scheduler in ["pa", "ffa", "csf"]
            }

            pa.append(futures["pa"].result())
            ffa.append(futures["ffa"].result())
            csf.append(futures["csf"].result())

            current_util = round(current_util + 0.1, 2)

    results = pd.DataFrame({
        "util": utilizations,
        "pa": pa,
        "ffa": ffa,
        "csf": csf,
    })

    results.to_csv(output_csv, index=False, sep=" ")


def main():
    if len(sys.argv) <= 1:
        print("Please pass the policies as an argument")
        return

    logs = sys.argv[1]

    process_logs(
        logs=logs,
        suffix=["no_delay", "no_delay", "no_delay"],
        output_csv="energy-no-delay.csv",
        output_img=f"energy_{logs}.png"
    )

    process_logs(
        logs=logs,
        suffix=["delay", "delay", "delay"],
        output_csv="energy-delay.csv",
        output_img=f"energy_{logs}.png"
    )

    process_logs(
        logs=logs,
        suffix=["delay", "timer", "timer"],
        output_csv="energy-timer.csv",
        output_img=f"energy_{logs}.png"
    )


if __name__ == "__main__":
    main()
