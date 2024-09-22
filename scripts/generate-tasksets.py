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


if __name__ == "__main__":
    main()
