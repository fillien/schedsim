#!/usr/bin/env python

import pandas as pd
import subprocess
import os
import sys
import io

SCHEDSIM = "./build/schedsim/schedsim"
SCHEDVIEW = "./build/schedview/schedview"

def main():
    if len(sys.argv) < 3:
        usage()
        sys.exit(1)

    # Check that the scenario is a valid file
    if not os.path.isfile(sys.argv[2]):
        sys.stdout.write("error: " + sys.argv[2] + " is not valid filepath\n")
        sys.stdout.flush()

    policies = sys.argv[1].split(',')
    
    results = pd.DataFrame()
    first = True

    for policy in policies:
        if first:
            results = run_scenario(SCHEDSIM, SCHEDVIEW, policy, sys.argv[2])
            first = False
        else:
            results_simu = run_scenario(SCHEDSIM, SCHEDVIEW, policy, sys.argv[2])
            results = merge(results, results_simu)
    
    results = results.drop_duplicates(ignore_index=True).ffill()
    results.to_csv("active_utilization.dat", index=False, sep=" ")

def usage():
    sys.stdout.write("Usage: " + sys.argv[0] + ": <policies> <scenario>\n")
    sys.stdout.flush()

def run_scenario(schedsim, schedview, sched_policy, scenario):
    logs_path = "logs.json"
    subprocess.run(
        [
            schedsim,
            "-s",
            scenario,
            "-p",
            str(sched_policy),
            "-o",
            logs_path,
        ],
        check=True,
    )
    output = subprocess.run(
        [schedview, logs_path, "--au"],
        capture_output=True,
        text=True,
    )
    result = pd.read_csv(io.StringIO(output.stdout.strip()), sep=' ')
    result.rename(columns={"active_utilization": sched_policy}, inplace=True)
    return result

def merge(df1, df2):
    return pd.merge(df1, df2, on='timestamp', how='outer')

if __name__ == "__main__":
    main()
