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

# %% editable=true slideshow={"slide_type": ""}
import build.python.pyschedlib as sc
import scripts.schedsim as sm

import os
import shutil
import plotly.graph_objects as go
import plotly.express as px
from plotly.subplots import make_subplots
import plotly.io as pio
import polars as pl
import subprocess
from concurrent.futures import ThreadPoolExecutor
from io import StringIO
import numpy as np
import json as js

pio.renderers.default = "iframe"
pl.Config.set_tbl_rows(-1)
pl.Config.set_tbl_cols(-1)

SCHEDVIEW = "./build/apps/schedview"
sim = sm.SchedSimRunner("./build/apps/schedsim")

# %%
PLATFORM = "platforms/exynos5422.json"
DIR = "alloc_tasksets"
UTILIZATION = 6.5
LITTLE_PERF_SCORE = 0.33334

configs = [
    ( "ff_little_first", "grub", 0.22 ),
    ( "ff_lb", "grub", 0.22 ),
    ( "ff_sma", "grub", 0.22 ),
    ( "ff_cap", "grub", 0.05 ),
    ( "ff_cap", "grub", 0.1 ),
    #( "ff_cap", "grub", 0.15 ),
    ( "ff_cap", "grub", 0.2 ),
    #( "ff_cap", "grub", 0.25 ),
    ( "ff_cap", "grub", 0.3 ),
    #( "ff_cap", "grub", 0.33334 ),
]

# %%
plat = {}
with open(PLATFORM, 'r') as f:
    plat = js.loads("".join(f.readlines()))

plat

# %% [markdown]
# # Generate the tasksets

# %%
if os.path.isdir(DIR):
    shutil.rmtree(DIR)

os.mkdir(DIR)

util_steps = list(reversed(range(1, int(UTILIZATION*10)+1, 2)))
NB_JOBS = 100
NB_TASK = 100
UMAX    = 0.3 #LITTLE_PERF_SCORE - 0.13334

for i in util_steps:
    data_path = f"{DIR}/{str(i)}"
    os.mkdir(data_path)
    utilization = round(i * 0.1, 1)
    print(f"jobs = {NB_JOBS}, tasks = {NB_TASK}, umax = {UMAX}, utilization = {utilization}")
    sc.generate_tasksets(data_path, NB_JOBS, NB_TASK, utilization, UMAX, success_rate = 1.0, compression_rate = 1.0, nb_cores = 16, a_special_need=(0.0, 0.22))

print("== finished ==")

# %%
bins = np.linspace(0, UMAX, num=61)
values = []
for i in range(1, 101):
    lines = []
    with open(f"{DIR}/65/{str(i)}.json", "r") as f:
        lines = f.readlines()
    file_values = [t.utilization for t in sc.from_json_setting(lines[0]).tasks]
    values += file_values

fig = px.histogram(x=values, nbins=60,
                   title="Distribution of task's utilization at total utilization 6.5")
fig.update_layout(xaxis_title="Value", yaxis_title="Frequency")
fig.update_traces(marker_line_width=1,marker_line_color="black", opacity=0.7)
fig.show()

values = []
for i in range(1, 101):
    lines = []
    with open(f"{DIR}/21/{str(i)}.json", "r") as f:
        lines = f.readlines()
    file_values = [t.utilization for t in sc.from_json_setting(lines[0]).tasks]
    values += file_values

fig = px.histogram(x=values, nbins=60,
                   title="Distribution of task's utilization at total utilization 2.1")
fig.update_layout(xaxis_title="Value", yaxis_title="Frequency")
fig.update_traces(marker_line_width=1,marker_line_color="black", opacity=0.7)
fig.show()

# %% [markdown]
# # Simulate the tasksets

# %%
for conf in configs:
    print(f"-> logs_{conf[0]}_{conf[1]}_{conf[2]}")
    sim.simul(DIR, conf[0], conf[1], PLATFORM, conf[2], f"{DIR}_logs_{conf[0]}_{conf[1]}_{conf[2]}")

print("== finished ==")

# %% [markdown]
# # Logs analysis

# %% editable=true slideshow={"slide_type": ""}
def compute_stats(logs_dir):
    args = [SCHEDVIEW, "--platform", PLATFORM, "-d", logs_dir, "--index", "--arrivals", "--rejected", "--deadlines-counts", "--cmigration", "--transitions", "--duration"]
    try:
        df_res = pl.read_csv(StringIO(subprocess.run(args, capture_output=True, text=True, check=True).stdout), separator=';')
    except subprocess.CalledProcessError as e:
        print(f"CalledProcessError: {str.join(" ", e.cmd)} | {e.stdout}")
        raise e

    df_res = df_res.with_columns((pl.col("file").str.extract(r"(\w+).json").cast(pl.Int32)).alias("id")).drop("file")
    df_energy = pl.DataFrame({
        "c1-energy": [],
        "c2-energy": []
    }, schema={"c1-energy": pl.Float64, "c2-energy": pl.Float64})
    for i in range(1, 101):
        args = [SCHEDVIEW, "--platform", PLATFORM, f"{logs_dir}/{i}.json", "--index", "--energy"]
        df = pl.read_csv(StringIO(subprocess.run(args, capture_output=True, text=True, check=True).stdout), separator=';')
        df = pl.DataFrame({"c1-energy": df["energy_consumption"][0], "c2-energy": df["energy_consumption"][1]})
        df_energy = pl.concat([df_energy, df])

    df_util = pl.DataFrame({
        "c1-util": [],
        "c2-util": []
    }, schema={"c1-util": pl.Float64, "c2-util": pl.Float64})
    for i in range(1, 101):
        args = [SCHEDVIEW, "--platform", PLATFORM, f"{logs_dir}/{i}.json", "--index", "--util"]
        df = pl.read_csv(StringIO(subprocess.run(args, capture_output=True, text=True, check=True).stdout), separator=';')
        df = pl.DataFrame({"c1-util": df["util"][0], "c2-util": df["util"][1]})
        df_util = pl.concat([df_util, df])

    return pl.concat([df_res, df_energy, df_util], how="horizontal")

stats = {}
for index,conf in enumerate(configs):
    print(f"-> {conf[0]} {conf[1]} {conf[2]}")
    stats[index] = {}
    stats_df = []

    def compute_utilization_step(i):
        return compute_stats(f"{DIR}_logs_{conf[0]}_{conf[1]}_{conf[2]}/{i}").with_columns(utilizations=i/10)

    with ThreadPoolExecutor() as executor:
        stats_df = list(executor.map(compute_utilization_step, util_steps))

    stats[index] = pl.concat(stats_df).select(["utilizations", "id", pl.exclude(["utilizations", "id"])]).sort(["utilizations", "id"])

print("== finished ==")

# %%
results = {}
for i,_ in enumerate(configs):
    results[i] = (stats[i].with_columns(
        (1 - (pl.col("rejected") / pl.col("arrivals"))).alias("accepted-rates"),
        (1 - (pl.col("deadlines-counts") / (pl.col("arrivals") - pl.col("rejected")))).alias("meet-rates"),
        (pl.col("cmigration") / pl.col("transitions")).alias("migration-rates"),
        (pl.col("c1-energy") / pl.col("duration")).alias("c1-power"),
        (pl.col("c2-energy") / pl.col("duration")).alias("c2-power"),
        (pl.col("c1-energy") / pl.col("duration") + pl.col("c2-energy") / pl.col("duration")).alias("total-power"),
        (pl.col("c1-util") + pl.col("c2-util")).alias("total-util")
    ).group_by("utilizations").agg(
        pl.col("accepted-rates").mean(),
        pl.col("meet-rates").mean(),
        pl.col("migration-rates").mean(),
        pl.col("c1-power").mean(),
        pl.col("c2-power").mean(),
        pl.col("total-power").mean(),
        pl.col("c1-util").mean(),
        pl.col("c2-util").mean(),
        pl.col("total-util").mean()
    ))

energy = pl.concat([
    stats[i].select(["utilizations", "id", "c1-energy", "c2-energy"]).rename({"c1-energy": f"c1-energy-{str(i)}", "c2-energy": f"c2-energy-{str(i)}"}) for i, _ in enumerate(configs)
], how="align")

energy_diff = energy.with_columns(
    [(pl.col(f"{clu}-energy-{str(i)}") - pl.col(f"{clu}-energy-{str(0)}")).alias(f"{clu}-energy-{str(i)}-diff") for i, _ in enumerate(configs) for clu in ["c1", "c2"]],
).with_columns(
    [(pl.col(f"c1-energy-{str(i)}-diff") + pl.col(f"c2-energy-{str(i)}-diff")).alias(f"energy-{str(i)}-diff") for i,_ in enumerate(configs)]
).group_by("utilizations").agg(
    pl.exclude(["utilizations", "id"]).mean()
)

# %%
plot_definitions = [
    # Row 1
    {'row': 1, 'col': 1, 'y_col': 'accepted-rates', 'y_scale': 100, 'y_range': [0, 105], 'title': 'Accepted Rates vs Utilizations', 'y_label': 'Accepted Rates (%)'},
    {'row': 1, 'col': 2, 'y_col': 'meet-rates', 'y_scale': 100, 'y_range': [0, 105], 'title': 'Deadline meet Rates vs Utilizations', 'y_label': 'Deadline meet Rates (%)'},
    {'row': 1, 'col': 3, 'y_col': 'migration-rates', 'y_scale': 100, 'y_range': [0, 105], 'title': 'Migration Rates vs Utilizations', 'y_label': 'Migration Rates (%)'},
    # Row 2
    {'row': 2, 'col': 1, 'y_col': 'c1-util', 'y_range_ref': 'total-util', 'title': 'Big Cluster Utilization', 'y_label': 'Average Total Utilization'},
    {'row': 2, 'col': 2, 'y_col': 'c2-util', 'y_range_ref': 'total-util', 'title': 'LITTLE Cluster Utilization', 'y_label': 'Average Total Utilization'},
    {'row': 2, 'col': 3, 'y_col': 'total-util', 'y_range_ref': 'total-util', 'title': 'Total Utilization', 'y_label': 'Average Total Utilization'},
    # Row 3
    {'row': 3, 'col': 1, 'y_col': 'c1-power', 'y_range_ref': 'c1-power', 'title': 'Power Consumption - Big Cluster', 'y_label': 'Average Power Consumption'},
    {'row': 3, 'col': 2, 'y_col': 'c2-power', 'y_range_ref': 'c1-power', 'title': 'Power Consumption - LITTLE Cluster', 'y_label': 'Average Power Consumption'},
    {'row': 3, 'col': 3, 'y_col': 'total-power', 'y_range_ref': 'total-power', 'title': 'Total Power Consumption', 'y_label': 'Average Power Consumption'},
]

energy_plot_definitions = [
    {'row': 4, 'col': 1, 'y_col': 'c1-energy-{i}-diff', 'title': 'Energy Diff - Big Cluster', 'y_label': 'Difference Energy Consumption'},
    {'row': 4, 'col': 2, 'y_col': 'c2-energy-{i}-diff', 'title': 'Energy Diff - LITTLE Cluster', 'y_label': 'Difference Energy Consumption'},
    {'row': 4, 'col': 3, 'y_col': 'energy-{i}-diff', 'title': 'Total Energy Difference', 'y_label': 'Difference Energy Consumption'},
]

subplot_titles = [p['title'] for p in plot_definitions] + [p['title'] for p in energy_plot_definitions]

config_labels = [f"{c[0]} {c[1]} {c[2]}" for c in configs]
colors = px.colors.qualitative.Plotly
color_map = {label: colors[i % len(colors)] for i, label in enumerate(config_labels)}

fig = make_subplots(rows=4, cols=3, subplot_titles=subplot_titles,
                    vertical_spacing=0.08, horizontal_spacing=0.08)

for i, c in enumerate(configs):
    config_label = f"{c[0]} {c[1]} {c[2]}"
    for p_idx, p in enumerate(plot_definitions):
        y_values = results[i][p['y_col']]
        if 'y_scale' in p:
            y_values = y_values * p['y_scale']

        fig.add_trace(go.Scatter(
            x=results[i]['utilizations'],
            y=y_values,
            name=config_label,
            legendgroup=config_label,
            showlegend=(p_idx == 0),
            mode='lines+markers', marker_symbol='square',
            marker_color=color_map[config_label],
            line_color=color_map[config_label]
        ), row=p['row'], col=p['col'])

for p in plot_definitions:
    y_range = p.get('y_range')
    if 'y_range_ref' in p:
        max_val = max([results[i][p['y_range_ref']].max() for i, _ in enumerate(configs)]) * 1.05
        y_range = [0, max_val]

    fig.update_xaxes(title_text="Utilizations", row=p['row'], col=p['col'])
    fig.update_yaxes(title_text=p['y_label'], range=y_range, row=p['row'], col=p['col'])


max_y_energy = energy_diff.select(pl.col(r"^energy.*diff$")).max().select(pl.max_horizontal("*"))[0,0] * 1.05
min_y_energy = energy_diff.select(pl.col(r"^energy.*diff$")).min().select(pl.min_horizontal("*"))[0,0] * 1.10
energy_yrange = [min_y_energy, max_y_energy]

for i, c in enumerate(configs):
    config_label = f"{c[0]} {c[1]} {c[2]}"
    for p in energy_plot_definitions:
        fig.add_trace(go.Scatter(
            x=energy_diff['utilizations'],
            y=energy_diff[p['y_col'].format(i=i)],
            name=config_label,
            legendgroup=config_label,
            showlegend=False,
            mode='lines+markers', marker_symbol='square',
            marker_color=color_map[config_label],
            line_color=color_map[config_label]
        ), row=p['row'], col=p['col'])

for p in energy_plot_definitions:
    fig.update_xaxes(title_text="Utilizations", row=p['row'], col=p['col'])
    fig.update_yaxes(title_text=p['y_label'], range=energy_yrange, row=p['row'], col=p['col'])

fig.update_layout(height=1800, width=2200, title_text="Simulation Results Analysis")
fig.show()

# %%
stats

# %%
