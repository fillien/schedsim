import os
import subprocess
from concurrent.futures import ThreadPoolExecutor
from io import StringIO
import numpy as np
import polars as pl
import matplotlib.pyplot as plt
import statistics
from IPython.display import display

SCHEDVIEW = "./build/schedview/schedview"

def run_scenario(schedview, platform, path, measures):
    args = [schedview, "--platform", platform, "--directory", path, "--index"]
    args += measures
    output = subprocess.run(
        args,
        capture_output=True,
        text=True,
        check=True,
    )
    return pl.read_csv(StringIO(output.stdout), separator=";")

def process_scheduler(scheduler, platform, directory, log_paths):
    """Process a scheduler's log directory to calculate mean power."""
    result = run_scenario(SCHEDVIEW, platform, os.path.join(log_paths[scheduler], directory), ["--energy", "--duration"])
    result = result.with_columns((pl.col("energy") / pl.col("duration")).alias("power"))
    return result

def normalize(data):
    max_value = max(data)
    min_value = min(data)
    normalized = []
    for value in data:
        normalized.append((value - min_value)/(max_value - min_value))
    
    return tuple(normalized)

def process_logs(logs, suffix, platform):
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
            utilizations.append(current_util)

            futures = {
                scheduler: executor.submit(process_scheduler, scheduler, platform, directory, log_paths)
                for scheduler in ["pa", "ffa", "csf"]
            }

            pa.append(futures["pa"].result())
            ffa.append(futures["ffa"].result())
            csf.append(futures["csf"].result())
            current_util = round(current_util + 0.1, 2)

    
    return pl.DataFrame({
        "util": utilizations,
        "pa": pa,
        "ffa": ffa,
        "csf": csf
    })

def compute(logs, hardware):
    df_no_delay = process_logs(logs, ["no_delay", "no_delay", "no_delay"], hardware)
    df_delay = process_logs(logs, ["delay", "delay", "delay"], hardware)
    df_timer = process_logs(logs, ["delay", "timer", "timer"], hardware)
    return (df_no_delay, df_delay, df_timer)