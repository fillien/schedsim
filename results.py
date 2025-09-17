# ---
# jupyter:
#   jupytext:
#     formats: ipynb,py:percent
#     text_representation:
#       extension: .py
#       format_name: percent
#       format_version: '1.3'
#       jupytext_version: 1.17.2
#   kernelspec:
#     display_name: Python 3 (ipykernel)
#     language: python
#     name: python3
# ---

# %%
import os
import shutil
import polars as pl
import glob

# %%
#overall = pl.read_csv("alloc-result.csv", separator=';', has_header=True)
#mcts = pl.read_csv("combined_mcts.csv", separator=',', has_header=True)
#nb_alloc = pl.read_csv("nb_alloc.csv", separator=';', has_header=True).with_columns(pl.lit("max").alias("alloc"))
#df = pl.concat([nb_alloc, overall, mcts], how="vertical")
df = pl.read_csv("min_taskset_result.csv", separator=';', has_header=False).rename({'column_1': 'taskset', 'column_2': 'alloc', 'column_3': 'result'})

# %%
df


# %%
def sort_taskset(df):
    return df.with_columns(
        taskset_num1=pl.col("taskset").str.extract(r"alloc_tasksets/(\d+)/\d+\.json$", 1).cast(pl.Int32),
        taskset_num2=pl.col("taskset").str.extract(r"alloc_tasksets/\d+/(\d+)\.json$", 1).cast(pl.Int32)
    ).sort(["taskset_num1", "taskset_num2"]).drop(["taskset_num1", "taskset_num2"])


df_filtered = df.group_by(["taskset", "alloc"]).agg(
    result=pl.col("result").min()
)
sorted_result = sort_taskset(df_filtered.pivot(
    index="taskset",
    on="alloc",
    values="result",
    aggregate_function="first"  # Use "first", "mean", etc., if duplicates persist
))

sorted_result.write_csv("all_taskset_result.csv")
sorted_result

# %%
