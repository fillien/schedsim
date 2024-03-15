#!/usr/bin/env python

import subprocess
import os
import shutil
from datetime import datetime

SCHEDSIM = "./build/schedsim/schedsim"
SCHEDGEN = "./build/schedgen/schedgen"
SCHEDVIEW = "./build/schedview/schedview"

NB_TASK = 10
TOTAL_NB_JOBS = 50

DATASET_DIR = f"data_{datetime.now().strftime('%Y-%m-%d-%H-%M-%S')}"
if os.path.exists(DATASET_DIR):
    shutil.rmtree(DATASET_DIR)
os.mkdir(DATASET_DIR)

for i in range(1, 31):
    utilization = i / 10.0
    name = str(utilization).replace('.', '_')
    current_sce = os.path.join(DATASET_DIR, f"util_{name}")
    os.mkdir(current_sce)

    for j in range(1, 16):
        subprocess.run([
            SCHEDGEN, "taskset", "-t", str(NB_TASK), "-j", str(TOTAL_NB_JOBS), "-s", "1",
            "-u", str(utilization), "-o", os.path.join(current_sce, f"{j}.json")
            ])


print(f"util energy")
utilisation = 0
for directory in os.listdir(DATASET_DIR):
    sum_energy = 0
    count = 0
    utilisation = round(utilisation + 0.1, 2)
    current_dir = os.path.join(DATASET_DIR, directory)
    if os.path.isdir(current_dir):
        for scenario in os.listdir(current_dir):
            count += 1
            logs_path = os.path.join(DATASET_DIR, "logs.json")
            subprocess.run([SCHEDSIM, "-s", os.path.join(current_dir, scenario), "-p", "grub", "-o", logs_path])
            result = subprocess.run([SCHEDVIEW, logs_path, "--cli", "--energy"], capture_output=True, text=True)
            sum_energy += float(result.stdout.strip())

    print(f"{utilisation} {round(sum_energy / count, 3)}")

