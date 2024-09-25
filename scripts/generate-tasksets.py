#!/usr/bin/env python

import pandas as pd
import subprocess
import os
import sys
import shutil
from datetime import datetime

SCHEDGEN = "./build/schedgen/schedgen"


def main():
    if len(sys.argv) <= 3:
        print("error: " + sys.argv[0] + " <nb_taskset> <nb_task> <nb_jobs>")
        return

    nb_taskset = int(sys.argv[1])
    nb_task = int(sys.argv[2])
    nb_jobs = int(sys.argv[3])

    period_min = 100
    period_max = 600

    # Create a directory to store taskset
    datadir = f"data_{datetime.now().strftime('%Y-%m-%d-%H-%M-%S')}"

    if os.path.exists(datadir):
        shutil.rmtree(datadir)
    os.mkdir(datadir)

    for i in range(1, 41):
        utilization = i / 10.0
        name = str(utilization).replace(".", "_")
        current_sce = os.path.join(datadir, f"util_{name}")
        os.mkdir(current_sce)

        for j in range(1, nb_taskset + 1):
            result = subprocess.run([
                "./taskgen.py",
                "-s",
                "1",
                "-n",
                str(nb_task),
                "-p",
                str(period_min),
                "-q",
                str(period_max),
                "-u",
                str(utilization)
                ], capture_output=True, text=True, check=True)

            with open("to_convet.txt", 'w') as file:
                file.write(result.stdout)

            subprocess.run(
                [
                    SCHEDGEN,
                    "convert",
                    "-i",
                    "to_convet.txt",
                    "-o",
                    os.path.join(current_sce, f"{j}.json"),
                ],
                check=True
            )


if __name__ == "__main__":
    main()
