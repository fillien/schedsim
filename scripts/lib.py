# TODO: Rewrite to use Python API (pyschedsim)
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


def call_cmpt(log, index):
    args = [SCHEDVIEW, "--platform", PLATFORM, log, "--frequency"]
    if index:
        args.append("--index")
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout

def read_csv_to_dataframe(csv_data):
    df = pl.read_csv(StringIO(csv_data), separator=';')
    df = df.with_columns(
        (pl.col('stop') - pl.col('start')).alias('duration')
    )

    return df

def compute_cluster_stats(df):
    distribution = df.group_by(['cluster_id', 'freq']).agg([
        pl.col('duration').sum().alias('total_duration'),
        pl.col('duration').mean().alias('avg_duration'),
        pl.col('duration').count().alias('count'),
        pl.col('duration').min().alias('min_duration'),
        pl.col('duration').max().alias('max_duration')
    ])

    return distribution

def combine_distributions(distribution_list):
    combined_distribution = (
        pl.concat(distribution_list)
        .group_by(['cluster_id', 'freq'])
        .agg([
            pl.col('total_duration').sum().alias('total_duration'),
            pl.col('count').sum().alias('count'),
            pl.col('min_duration').min().alias('min_duration'),
            pl.col('max_duration').max().alias('max_duration'),
            (pl.col('total_duration').sum() / pl.col('count').sum()).alias('avg_duration')
        ])
        .sort(['cluster_id', 'freq'])
    )

    return combined_distribution

def compute_avg_freq_by_utilization(results):
    avg_freq_data = []

    for util_step, distribution in enumerate(results, start=1):
        utilization = util_step * 0.1
        avg_freq_by_cluster = (
            distribution
            .group_by('cluster_id')
            .agg([
                ((pl.col('freq') * pl.col('total_duration')).sum() /
                 pl.col('total_duration').sum()).alias('avg_frequency'),
                pl.col('total_duration').sum().alias('total_duration_sum'),
                pl.col('count').sum().alias('total_count')
            ])
        )

        avg_freq_by_cluster = avg_freq_by_cluster.with_columns(
            pl.lit(utilization).alias('utilization')
        )
        avg_freq_data.append(avg_freq_by_cluster)

    final_df = (
        pl.concat(avg_freq_data)
        .select([
            'utilization',
            'cluster_id',
            'avg_frequency',
            'total_duration_sum',
            'total_count'
        ])
        .sort(['utilization', 'cluster_id'])
    )

    return final_df

def average_freq(logs_name):
    results = []
    UTILIZATION = 6.5
    util_steps = range(1, int(UTILIZATION*10)+1, 2)
    for j in util_steps:
        util_results = []
        for i in range(1, 101):
            target = f"{logs_name}/{j}/{i}.json"
            raw_data = read_csv_to_dataframe(call_cmpt(target, True))
            util_results.append(compute_cluster_stats(raw_data))

        results.append(combine_distributions(util_results))

    return compute_avg_freq_by_utilization(results)



# %%
# allocations = ["smart_ass"]
# schedulers = ["grub"]

# combinations = list(itertools.product(allocations, schedulers))

# def worker(alloc, sched):
#     freq = average_freq(f"{DIR}_logs_{alloc}_{sched}").rename({"avg_frequency": f"avg_frequency_{alloc}_{sched}"})
#     return (alloc, sched, freq)

# with concurrent.futures.ThreadPoolExecutor() as executor:
#     futures = [executor.submit(worker, alloc, sched) for alloc, sched in combinations]
#     results = [future.result() for future in futures]

# freqs = {}
# for alloc, sched, freq in results:
#     if alloc not in freqs:
#         freqs[alloc] = {}
#     freqs[alloc][sched] = freq


# %%
# for alloc, v in freqs.items():
#     for sched, w in v.items():
#         freqs[alloc][sched] = w.with_columns((pl.arange(1, w.height + 1)).alias("id"))

# flatten_freqs = [value for subdict in freqs.values() for value in subdict.values()]

# def fusion(left, right):
#     return left.join(right, on="id", how="inner").select(
#         pl.col("id"), pl.col("utilization"), pl.col("cluster_id"), pl.selectors.matches("avg_frequency")
#     )

# avg_freqs = reduce(fusion, flatten_freqs).select(pl.all().exclude("id"))

# %%
# plt.figure(figsize=(15, 6))

# sched = "ffa"

# fl = avg_freqs.filter(pl.col("cluster_id") == 1)
# plt.subplot(1, 2, 1)
# plt.plot(fl["utilization"], fl[f"avg_frequency_little_first_{sched}"], label="Little First", marker='o')
# plt.plot(fl["utilization"], fl[f"avg_frequency_big_first_{sched}"], label="Big First", marker='s')
# plt.plot(fl["utilization"], fl[f"avg_frequency_smart_ass_{sched}"], label="Smart Alloc", marker='^')
# plt.xlabel("Utilization")
# plt.ylabel("Average Frequency")
# plt.title("Big Only")
# plt.legend()
# plt.grid(True)

# fl = avg_freqs.filter(pl.col("cluster_id") == 2)
# plt.subplot(1, 2, 2)
# plt.plot(fl["utilization"], fl[f"avg_frequency_little_first_{sched}"], label="Little First", marker='o')
# plt.plot(fl["utilization"], fl[f"avg_frequency_big_first_{sched}"], label="Big First", marker='s')
# plt.plot(fl["utilization"], fl[f"avg_frequency_smart_ass_{sched}"], label="Smart Alloc", marker='^')
# plt.xlabel("Utilization")
# plt.ylabel("Average Frequency")
# plt.title("Little Only")
# plt.legend()
# plt.grid(True)

# plt.tight_layout()
# plt.show()
