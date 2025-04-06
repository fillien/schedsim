#!/usr/bin/env python

import subprocess
import os
import sys
import shutil
import concurrent.futures

SCHEDSIM = "./build/schedsim/schedsim"
PLATFORM = "./platforms/exynos5422LITTLE.json"

def main():
    if len(sys.argv) <= 2:
        print("error: " + sys.argv[0] + " <tasksets> <sched_policy>")
        return

    datadir = sys.argv[1]
    sched_policy = sys.argv[2]

    logs = datadir + "_logs_" + sched_policy
    if os.path.isdir(logs):
        shutil.rmtree(logs)

    os.mkdir(logs)
    for directory in sorted(os.listdir(datadir)):
        current_dir = os.path.join(datadir, directory)
        logs_dir = os.path.join(logs, directory)
        print(current_dir)
        if not os.path.isdir(logs_dir):
            os.mkdir(logs_dir)

        if os.path.isdir(current_dir):
            scenarios = sorted(os.listdir(current_dir))
            with concurrent.futures.ThreadPoolExecutor() as executor:
                futures = [
                    executor.submit(
                        run_scenario,
                        SCHEDSIM,
                        current_dir,
                        scenario,
                        sched_policy,
                        datadir,
                        logs_dir,
                    )
                    for scenario in scenarios
                ]
                for future in concurrent.futures.as_completed(futures):
                    future.result()


def run_scenario(schedsim, current_dir, scenario, sched_policy, datadir, log_dir):
    result = subprocess.run(
        [
            schedsim,
            "-s",
            os.path.join(current_dir, scenario),
            "-p",
            PLATFORM,
            "--sched",
            str(sched_policy),
            "-o",
            os.path.join(log_dir, scenario),
        ],
        check=True,
    )
    return result


if __name__ == "__main__":
    main()
