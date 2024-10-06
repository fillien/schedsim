#!/usr/bin/env python

import pandas as pd
import subprocess
import os
import sys
import io

SCHEDVIEW = "./build/schedview/schedview"


def main():
    if len(sys.argv) == 2:
        results = onefile(sys.argv[1])

    elif len(sys.argv) == 4:
        results = multiplefile(sys.argv[1], sys.argv[2], sys.argv[3])

    else:
        usage()
        sys.exit(1)

    results.to_csv("active_utilization.dat", index=False, sep=" ")
    call_gnuplot()


def onefile(log_file):
    if not os.path.isfile(log_file):
        sys.stdout.write("error: " + log_file + " is not valid filepath\n")
        sys.stdout.flush()
        sys.exit(1)

    output = subprocess.run(
        [SCHEDVIEW, log_file, "--au"],
        capture_output=True,
        text=True,
    )

    return pd.read_csv(io.StringIO(output.stdout.strip()), sep=" ")


def multiplefile(logs, utilisation, scenario):
    log_paths = {}
    log_paths["grub"] = f"{logs}_logs_grub/{utilisation}/{scenario}"
    log_paths["pa"] = f"{logs}_logs_pa/{utilisation}/{scenario}"
    log_paths["pa_f_min"] = f"{logs}_logs_pa_f_min/{utilisation}/{scenario}"
    log_paths["pa_m_min"] = f"{logs}_logs_pa_m_min/{utilisation}/{scenario}"

    results = pd.DataFrame()
    first = True
    for path in log_paths.items():
        if first:
            results = onefile(path[1])
            results.rename(columns={"active_utilization": path[0]}, inplace=True)
            first = False
        else:
            result = onefile(path[1])
            result.rename(columns={"active_utilization": path[0]}, inplace=True)
            results = merge(results, result)

        results = results.drop_duplicates(ignore_index=True).ffill()

    return results


def merge(df1, df2):
    return pd.merge(df1, df2, on="timestamp", how="outer")


def call_gnuplot():
    output = subprocess.run(["gnuplot", "scripts/plot-active-utilization.gp"])


def usage():
    sys.stdout.write("Usage: " + sys.argv[0] + ": <scenario>\n")
    sys.stdout.flush()


if __name__ == "__main__":
    main()
