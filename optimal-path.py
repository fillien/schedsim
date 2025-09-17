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

pl.Config.set_tbl_rows(-1)
pl.Config.set_tbl_cols(-1)


# %%
#files = glob.glob('mcts-*.csv')
#print(files)

#dfs = []

#for file in files:
#    df = pl.read_csv(file, separator=';', has_header=False)
#    df = df.rename({'column_1': 'taskset', 'column_2': 'alloc', 'column_3': 'result'})
#    dfs.append(df)

#combined = pl.concat(dfs).sort('taskset')

#combined.write_csv('combined_mcts.csv')

# %%
def sort_taskset(df):
    return df.with_columns(
        taskset_num1=pl.col("taskset").str.extract(r"alloc_tasksets/(\d+)/\d+\.json$", 1).cast(pl.Int32),
        taskset_num2=pl.col("taskset").str.extract(r"alloc_tasksets/\d+/(\d+)\.json$", 1).cast(pl.Int32)
    ).sort(["taskset_num1", "taskset_num2"]).drop(["taskset_num1", "taskset_num2"])


#removed_dep = combined.group_by("taskset").agg(
#    mcts=pl.col("mcts").min()
#)

#removed_dep = sort_taskset(removed_dep)
#removed_dep

# %%
def list_files_walk(start_path='min_tasksets'):
    return [
        os.path.join(root, file)
        for root, dirs, files in os.walk(start_path)
        for file in files
    ]

files = list_files_walk()

df = pl.DataFrame({"taskset": files})

sim_cols = ["ff_lf", "ff_lb", "ff_sma"]
df = df.with_columns([pl.Series(col, [None] * df.height) for col in sim_cols])
df = sort_taskset(df)

# %%
#df = df.update(removed_dep, on="taskset")
#df = df.filter(
#    pl.col("taskset").str.contains(r".*/(?:[1-9]|10)\.json$")
#)
#df

# %%
## Detect the missing values

long_df = df.unpivot(
    index=["taskset"],
    on=df.columns[1:],
    variable_name="simulation",
    value_name="result"
)

def get_args_from_col(col: str) -> str:
    cols = {
        "mcts": "--sched grub --alloc opti",
        "ff_lf": "--sched grub --alloc ff_little_first",
        "ff_lb": "--sched grub --alloc ff_lb",
        "ff_sma": "--sched grub --alloc ff_sma",
        "ff_cap_0.05": "--sched grub --alloc ff_cap"
    }
    return cols[col]

missing = long_df.filter(pl.col("result").is_null())

with open("missing.sh", "w") as f:
    pass

with open("missing.sh", "w") as f:
    for row in missing.iter_rows(named=True):
        taskset = row["taskset"]
        mcts = row.get("mcts", None)
        ff_sma = row.get("ff_sma", None)
        formatted_line = f"./build/apps/alloc --platform platforms/exynos5422.json {get_args_from_col(row["simulation"])} --output logs.json --input {row["taskset"]}\n"
        f.write(formatted_line)

# %%
