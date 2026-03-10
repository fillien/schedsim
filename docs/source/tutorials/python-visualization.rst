Visualizing Simulation Results
==============================

This tutorial demonstrates how to visualize scheduling simulation results
using matplotlib. You will learn how to produce response time histograms,
frequency time-series plots, active-core count charts, and energy breakdown
bar charts --- all from a single simulation run.

Prerequisites
-------------

.. note::

   This tutorial requires matplotlib (``pip install matplotlib``). All other
   tutorials work without external Python dependencies.

Running a Simulation
--------------------

Set up a four-core platform with GRUB and FFA, enable energy tracking, and
collect the trace in memory. The same ``MemoryTraceWriter`` is used for both
metrics and time-series analysis.

.. code-block:: python

   import pyschedsim as sim

   engine   = sim.Engine()
   platform = engine.platform

   cpu   = platform.add_processor_type("cpu", 1.0)
   clock = platform.add_clock_domain(500.0, 2000.0, 0.0001)  # 100 us transition
   power = platform.add_power_domain([
       (0, 0, 0.0,   100.0),
       (1, 0, 0.001,  10.0),
   ])
   for _ in range(4):
       platform.add_processor(cpu, clock, power)

   # Generate a scenario: 6 tasks with utilizations summing to 2.4
   utils    = [0.25, 0.20, 0.15, 0.35, 0.30, 0.25]
   scenario = sim.from_utilizations(utils, seed=42)
   tasks    = sim.inject_scenario(engine, scenario)

   engine.enable_energy_tracking(True)
   platform.finalize()

   for i, tp in enumerate(scenario.tasks):
       sim.schedule_arrivals(engine, tasks[i], tp.jobs)

   procs     = sim.get_all_processors(engine)
   scheduler = sim.EdfScheduler(engine, procs)
   for task in tasks:
       scheduler.add_server(task)
   scheduler.enable_grub()
   scheduler.enable_ffa(0.001, 1)   # FFA: frequency-first DVFS + DPM

   allocator = sim.SingleSchedulerAllocator(engine, scheduler)
   writer    = sim.MemoryTraceWriter()
   engine.set_trace_writer(writer)
   engine.run()

   metrics = writer.compute_metrics()

Response Time Distribution
---------------------------

Plot a histogram of response times for each task. Using one subplot per task
keeps the chart readable when tasks have very different time scales.

.. code-block:: python

   import matplotlib.pyplot as plt

   plt.style.use("seaborn-v0_8-whitegrid")

   rt_map  = metrics.get_response_times_per_task()
   n_tasks = len(rt_map)

   fig, axes = plt.subplots(1, n_tasks, figsize=(3 * n_tasks, 3),
                             sharey=True)

   for ax, (task_id, times) in zip(axes, sorted(rt_map.items())):
       times_ms = [t * 1e3 for t in times]          # convert to milliseconds
       ax.hist(times_ms, bins=20, edgecolor="white")
       ax.set_title(f"Task {task_id}")
       ax.set_xlabel("Response time (ms)")
       ax.set_ylabel("Count" if ax is axes[0] else "")

   fig.suptitle("Response Time Distribution per Task", y=1.02)
   plt.tight_layout()
   plt.savefig("response_times.png", dpi=150)
   plt.show()

Frequency Time-Series
----------------------

``writer.track_frequency_changes()`` returns a list of ``FrequencyInterval``
objects, each with ``start``, ``stop``, ``cluster_id``, and ``frequency``
attributes. Use a step plot to visualise the piecewise-constant signal.

.. code-block:: python

   freq_intervals = writer.track_frequency_changes()

   # Collect one series per cluster
   from collections import defaultdict
   cluster_freqs = defaultdict(lambda: {"times": [], "freqs": []})

   for iv in freq_intervals:
       series = cluster_freqs[iv.cluster_id]
       series["times"].extend([iv.start, iv.stop])
       series["freqs"].extend([iv.frequency, iv.frequency])

   fig, ax = plt.subplots(figsize=(10, 3))

   for cluster_id, series in sorted(cluster_freqs.items()):
       ax.step(series["times"], series["freqs"],
               where="post", label=f"Cluster {cluster_id}")

   ax.set_xlabel("Time (s)")
   ax.set_ylabel("Frequency (MHz)")
   ax.set_title("DVFS Frequency Over Time")
   ax.legend(loc="upper right")
   ax.set_ylim(0)
   plt.tight_layout()
   plt.savefig("frequency_trace.png", dpi=150)
   plt.show()

Active Core Count Over Time
----------------------------

``writer.track_core_changes()`` returns ``CoreCountInterval`` objects with
``start``, ``stop``, ``cluster_id``, and ``active_cores`` attributes. This
shows how the DPM policy powers down idle processors.

.. code-block:: python

   core_intervals = writer.track_core_changes()

   cluster_cores = defaultdict(lambda: {"times": [], "counts": []})

   for iv in core_intervals:
       series = cluster_cores[iv.cluster_id]
       series["times"].extend([iv.start, iv.stop])
       series["counts"].extend([iv.active_cores, iv.active_cores])

   fig, ax = plt.subplots(figsize=(10, 3))

   for cluster_id, series in sorted(cluster_cores.items()):
       ax.step(series["times"], series["counts"],
               where="post", label=f"Cluster {cluster_id}")

   ax.set_xlabel("Time (s)")
   ax.set_ylabel("Active cores")
   ax.set_title("Active Core Count Over Time")
   ax.yaxis.get_major_locator().set_params(integer=True)
   ax.legend(loc="upper right")
   ax.set_ylim(0)
   plt.tight_layout()
   plt.savefig("core_trace.png", dpi=150)
   plt.show()

Energy Breakdown
----------------

``metrics.get_energy_per_processor()`` returns a dict mapping processor ID
to energy in millijoules. A horizontal bar chart makes per-core differences
easy to compare.

.. code-block:: python

   energy_map = metrics.get_energy_per_processor()
   proc_ids   = sorted(energy_map.keys())
   energies   = [energy_map[p] for p in proc_ids]
   labels     = [f"Proc {p}" for p in proc_ids]

   fig, ax = plt.subplots(figsize=(6, 3))
   bars = ax.barh(labels, energies, color="steelblue", edgecolor="white")
   ax.bar_label(bars, fmt="%.1f mJ", padding=4)
   ax.set_xlabel("Energy (mJ)")
   ax.set_title("Per-Processor Energy Consumption")
   ax.set_xlim(0, max(energies) * 1.2)
   plt.tight_layout()
   plt.savefig("energy_breakdown.png", dpi=150)
   plt.show()

Combined Dashboard
------------------

Assemble all four charts into a single 2×2 figure for papers or reports.

.. code-block:: python

   fig, axes = plt.subplots(2, 2, figsize=(12, 8))
   fig.suptitle("Simulation Dashboard", fontsize=14, fontweight="bold")

   # ── Top-left: response times (task 1 only for brevity) ───────────────────
   ax = axes[0, 0]
   first_task_id = sorted(rt_map.keys())[0]
   times_ms = [t * 1e3 for t in rt_map[first_task_id]]
   ax.hist(times_ms, bins=20, color="steelblue", edgecolor="white")
   ax.set_title(f"Response Times — Task {first_task_id}")
   ax.set_xlabel("Response time (ms)")
   ax.set_ylabel("Count")

   # ── Top-right: frequency trace ────────────────────────────────────────────
   ax = axes[0, 1]
   for cluster_id, series in sorted(cluster_freqs.items()):
       ax.step(series["times"], series["freqs"],
               where="post", label=f"Cluster {cluster_id}")
   ax.set_title("DVFS Frequency")
   ax.set_xlabel("Time (s)")
   ax.set_ylabel("Frequency (MHz)")
   ax.set_ylim(0)
   ax.legend(fontsize=8)

   # ── Bottom-left: active core count ────────────────────────────────────────
   ax = axes[1, 0]
   for cluster_id, series in sorted(cluster_cores.items()):
       ax.step(series["times"], series["counts"],
               where="post", label=f"Cluster {cluster_id}")
   ax.set_title("Active Core Count")
   ax.set_xlabel("Time (s)")
   ax.set_ylabel("Active cores")
   ax.yaxis.get_major_locator().set_params(integer=True)
   ax.set_ylim(0)
   ax.legend(fontsize=8)

   # ── Bottom-right: energy breakdown ────────────────────────────────────────
   ax = axes[1, 1]
   bars = ax.barh(labels, energies, color="steelblue", edgecolor="white")
   ax.bar_label(bars, fmt="%.1f mJ", padding=4)
   ax.set_title("Per-Processor Energy")
   ax.set_xlabel("Energy (mJ)")
   ax.set_xlim(0, max(energies) * 1.2)

   plt.tight_layout()
   plt.savefig("dashboard.pdf")
   plt.savefig("dashboard.png", dpi=150)
   plt.show()

Saving Figures
--------------

.. code-block:: python

   # Vector PDF for publication (LaTeX-ready)
   plt.savefig("results.pdf")

   # Raster PNG for slides or reports
   plt.savefig("results.png", dpi=150)

   # High-DPI raster for posters
   plt.savefig("results.png", dpi=300, bbox_inches="tight")

Use ``bbox_inches="tight"`` to prevent axis labels from being clipped at the
figure boundary.

Without matplotlib
------------------

If matplotlib is not available, the same data can be printed as plain text.
This is useful in headless environments or CI pipelines.

.. code-block:: python

   # Response time summary (no plotting)
   rt_map = metrics.get_response_times_per_task()
   print("Response time summary")
   print(f"{'Task':>6}  {'N':>5}  {'min ms':>8}  {'mean ms':>8}  "
         f"{'max ms':>8}  {'p95 ms':>8}")
   print("-" * 52)
   for task_id, times in sorted(rt_map.items()):
       stats = sim.compute_response_time_stats(times)
       print(f"{task_id:>6}  {len(times):>5}  "
             f"{stats.min*1e3:>8.2f}  {stats.mean*1e3:>8.2f}  "
             f"{stats.max*1e3:>8.2f}  {stats.percentile_95*1e3:>8.2f}")

   # Frequency intervals as text
   print("\nFrequency intervals")
   print(f"{'cluster':>8}  {'start s':>10}  {'stop s':>10}  {'freq MHz':>9}")
   print("-" * 44)
   for iv in writer.track_frequency_changes():
       print(f"{iv.cluster_id:>8}  {iv.start:>10.6f}  "
             f"{iv.stop:>10.6f}  {iv.frequency:>9.0f}")

   # Energy per processor
   print("\nPer-processor energy")
   for proc_id, energy in sorted(metrics.get_energy_per_processor().items()):
       print(f"  Processor {proc_id}: {energy:.2f} mJ")

Next Steps
----------

- :doc:`analysis` --- full reference for metrics and time-series analysis
- :doc:`python-batch` --- generate the data for multi-line plots from sweeps
