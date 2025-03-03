#!/usr/bin/env python

import subprocess
import os
import sys
import shutil
import concurrent.futures

SCHEDSIM = "./build/schedsim/schedsim"
PLATFORM = "./platforms/exynos5422.json"


def main(datadir, alloc, sched_policy, delay, suffix=""):
    logs = datadir + "_logs_" + alloc + "_" + sched_policy + suffix
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
                        alloc,
                        sched_policy,
                        datadir,
                        logs_dir,
                        delay
                    )
                    for scenario in scenarios
                ]
                for future in concurrent.futures.as_completed(futures):
                    future.result()

def run_scenario(schedsim, current_dir, scenario, alloc_policy, sched_policy, datadir, log_dir, delay):
    env = os.environ.copy()
    env["MallocNanoZone"] = "0"
    command = [
        schedsim,
        "-s",
        os.path.join(current_dir, scenario),
        "-p",
        PLATFORM,
        "--alloc",
        str(alloc_policy),
        "--sched",
        str(sched_policy),
        "-o",
        os.path.join(log_dir, scenario),
    ]
    if delay:
        command.extend(["--delay", "true"])

    return subprocess.run(command, check=True, env=env)


if __name__ == "__main__":
    if len(sys.argv) <= 3:
        print("error: " + sys.argv[0] + " <tasksets> <alloc_policy> <sched_policy> <delay>")
        exit()

    datadir = sys.argv[1]
    alloc_policy = sys.argv[2]
    sched_policy = sys.argv[3]
    delay = sys.argv[4]

    if delay == "true":
        print("delay is true")
        delay = True
    else:
        print("delay is false")
        delay = False

    main(datadir, alloc_policy, sched_policy, delay)
