#!/usr/bin/env python

import pandas as pd
import subprocess
import os
import sys
import shutil
import concurrent.futures
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

    log_paths = {}
    log_paths["grub"] = f"{logs}_logs_grub"
    log_paths["pa"] = f"{logs}_logs_pa"
    log_paths["ffa"] = f"{logs}_logs_ffa"
    log_paths["csf"] = f"{logs}_logs_csf"

    current_util = 0.1
    for directory in sorted(os.listdir(log_paths["grub"])):
        print (directory)
        utilizations.append(current_util)
        grub.append(run_scenario(SCHEDVIEW, os.path.join(log_paths["grub"], directory))["power"].mean())
        pa.append(run_scenario(SCHEDVIEW, os.path.join(log_paths["pa"], directory))["power"].mean())
        ffa.append(run_scenario(SCHEDVIEW, os.path.join(log_paths["ffa"], directory))["power"].mean())
        csf.append(run_scenario(SCHEDVIEW, os.path.join(log_paths["csf"], directory))["power"].mean())
        current_util = round(current_util + 0.1, 2)

    
    results = pd.DataFrame()
    results["util"] = utilizations
    results["grub"] = grub
    results["pa"] = pa
    results["ffa"] = ffa
    results["csf"] = csf

    results.to_csv("data-energy.csv", index=False, sep=" ")
    # subprocess.run(["gnuplot", "./scripts/plot.gp"])


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


def merge(df1, df2):
    return pd.merge(df1, df2, on="utilization")


if __name__ == "__main__":
    main()
