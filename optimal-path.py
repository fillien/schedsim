# ---
# jupyter:
#   jupytext:
#     formats: ipynb,py:percent
#     text_representation:
#       extension: .py
#       format_name: percent
#       format_version: '1.3'
#       jupytext_version: 1.16.6
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

files = glob.glob('mcts-*.csv')
print(files)

dfs = []

for file in files:
    df = pl.read_csv(file, separator=';', has_header=False)
    df = df.rename({'column_1': 'taskset', 'column_2': 'status', 'column_3': 'mcts'})
    dfs.append(df)

combined = pl.concat(dfs).sort('taskset')

combined.write_csv('combined_mcts.csv')

# %%
combined

# %%
set(range(1, 66, 2)) - set(sorted(set([int(i.split("/")[1]) for i in combined["taskset"]])))
