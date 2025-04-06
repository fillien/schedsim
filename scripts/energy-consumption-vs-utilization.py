#!/usr/bin/env python

import pandas as pd
import subprocess
import os
import sys
from concurrent.futures import ThreadPoolExecutor
from io import StringIO

SCHEDVIEW = "./build/schedview/schedview"

def main():
    if len(sys.argv) <= 1:
        print("Please pass the tasksets as an argument")
        return

    PLATFORM = sys.argv[2]
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
        return run_scenario(SCHEDVIEW, os.path.join(log_paths[scheduler], directory), PLATFORM)["power"].mean()

    current_util = 0.1

    with ThreadPoolExecutor() as executor:
        folders = os.listdir(log_paths["grub"])
        if ".DS_Store" in folders:
            folders.remove(".DS_Store")

        for directory in sorted(folders):
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

    results.to_csv(sys.argv[2], index=False, sep=" ")


def run_scenario(schedview, logs_path, platform):
    output = subprocess.run(
        [SCHEDVIEW, "--platform", platform, "--directory", logs_path, "--energy", "--duration", "--index"],
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
