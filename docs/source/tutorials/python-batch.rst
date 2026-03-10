Parameter Sweeps and Policy Comparison
=======================================

The Python API makes it easy to run batch simulations, sweep parameters, and
compare scheduling policies programmatically. Because each ``Engine`` is an
independent object, you can run hundreds of simulations in a plain loop without
any global state or process isolation.

Prerequisites
-------------

This tutorial assumes familiarity with the basics covered in
:doc:`python-quickstart` and :doc:`analysis`.

.. admonition:: Simulation state is not reusable

   An ``Engine`` that has been run cannot be reset and run again. **Always
   create a fresh ``Engine`` (and a fresh ``Platform``, ``Scheduler``, etc.)
   for each simulation run.** The helper function pattern below enforces this
   naturally.

Helper Function
---------------

Encapsulate the full simulation setup in a single function that accepts the
parameters you want to vary and returns a metrics object. This keeps each run
isolated and the call site concise.

.. code-block:: python

   import pyschedsim as sim

   def run_simulation(
       utilizations,        # list of per-task utilizations, e.g. [0.3, 0.25, 0.2, 0.15]
       reclamation="grub",  # "grub" or "cash"
       dvfs_policy=None,    # None, "pa", "ffa", or "csf"
       cooldown=0.001,      # DVFS cooldown in seconds
       cstate=1,            # target C-state for DPM
       seed=42,
   ):
       """Run one simulation and return the SimulationMetrics."""
       # ── Fresh engine and platform every call ──────────────────────────────
       engine   = sim.Engine()
       platform = engine.platform

       cpu   = platform.add_processor_type("cpu", 1.0)
       clock = platform.add_clock_domain(500.0, 2000.0)
       power = platform.add_power_domain([
           (0, 0, 0.0,   100.0),
           (1, 0, 0.001,  10.0),
       ])
       for _ in range(4):
           platform.add_processor(cpu, clock, power)

       # ── Generate scenario from per-task utilizations ──────────────────────
       # Each value must be in (0, 1].  Total may exceed 1 (multi-processor).
       scenario = sim.from_utilizations(utilizations, seed=seed)
       tasks    = sim.inject_scenario(engine, scenario)

       # Enable energy tracking before finalize (required for energy metrics)
       if dvfs_policy is not None:
           engine.enable_energy_tracking(True)

       platform.finalize()

       # Schedule job arrivals after inject and finalize
       for i, tp in enumerate(scenario.tasks):
           sim.schedule_arrivals(engine, tasks[i], tp.jobs)

       # ── Scheduler ─────────────────────────────────────────────────────────
       procs     = sim.get_all_processors(engine)
       scheduler = sim.EdfScheduler(engine, procs)
       for task in tasks:
           scheduler.add_server(task)

       # Reclamation policy (always required before DVFS)
       if reclamation == "grub":
           scheduler.enable_grub()
       elif reclamation == "cash":
           scheduler.enable_cash()

       # Optional DVFS/DPM policy
       if dvfs_policy == "pa":
           scheduler.enable_power_aware_dvfs(cooldown)
           scheduler.enable_basic_dpm(cstate)
       elif dvfs_policy == "ffa":
           scheduler.enable_ffa(cooldown, cstate)
       elif dvfs_policy == "csf":
           scheduler.enable_csf(cooldown, cstate)

       allocator = sim.SingleSchedulerAllocator(engine, scheduler)

       # ── Run ───────────────────────────────────────────────────────────────
       writer = sim.MemoryTraceWriter()
       engine.set_trace_writer(writer)
       engine.run()

       return writer.compute_metrics()

.. note::

   ``sim.from_utilizations()`` generates a ``ScenarioData`` object with
   log-uniform periods and Weibull-distributed job execution times. Each element
   of ``utilizations`` must be in ``(0, 1]``. The total utilization across all
   tasks may exceed 1.0 for multi-processor experiments.

Utilization Sweep
-----------------

Vary total utilization from lightly loaded to near-capacity and observe how
deadline miss rate evolves. The four-core platform has a capacity of 4.0;
here we sweep from 1.0 to 3.95.

.. code-block:: python

   # Distribute total utilization evenly across 8 tasks.
   # Each individual task utilization stays below 1.0.
   total_utils = [1.0, 2.0, 3.0, 3.5, 3.8, 3.9, 3.95]
   n_tasks = 8

   print(f"{'Total U':>8}  {'Completed':>10}  {'Misses':>8}  {'Miss rate':>10}")
   print("-" * 42)

   for total_u in total_utils:
       per_task_u = total_u / n_tasks
       utils = [per_task_u] * n_tasks

       metrics = run_simulation(utils, reclamation="grub", seed=0)

       miss_rate = (metrics.deadline_misses / metrics.total_jobs
                    if metrics.total_jobs > 0 else 0.0)
       print(f"{total_u:>8.2f}  "
             f"{metrics.completed_jobs:>5}/{metrics.total_jobs:<5}  "
             f"{metrics.deadline_misses:>8}  "
             f"{miss_rate:>9.2%}")

Policy Comparison
-----------------

For a fixed workload at moderate load, compare GRUB and CASH side by side.

.. code-block:: python

   n_tasks = 6
   total_u = 3.0
   per_task_u = total_u / n_tasks
   utils = [per_task_u] * n_tasks

   results = {}
   for policy in ("grub", "cash"):
       results[policy] = run_simulation(utils, reclamation=policy, seed=42)

   print(f"{'Policy':>6}  {'Completed':>10}  {'Misses':>8}  {'Preemptions':>12}")
   print("-" * 44)
   for policy, m in results.items():
       print(f"{policy:>6}  "
             f"{m.completed_jobs:>5}/{m.total_jobs:<5}  "
             f"{m.deadline_misses:>8}  "
             f"{m.preemptions:>12}")

DVFS Policy Comparison
-----------------------

Enable energy tracking and compare four energy management strategies for the
same workload. The ``dvfs_policy=None`` baseline uses GRUB only, with no
frequency scaling.

.. code-block:: python

   n_tasks = 6
   utils = [0.45] * n_tasks  # total U = 2.7 on a 4-core platform

   dvfs_configs = [
       ("No DVFS",       None),
       ("Power-Aware",   "pa"),
       ("FFA",           "ffa"),
       ("CSF",           "csf"),
   ]

   print(f"{'Policy':>14}  {'Energy (mJ)':>12}  {'Misses':>8}  {'Freq changes':>13}")
   print("-" * 54)

   for label, dvfs in dvfs_configs:
       m = run_simulation(
           utils,
           reclamation="grub",
           dvfs_policy=dvfs,
           cooldown=0.001,
           seed=7,
       )
       print(f"{label:>14}  "
             f"{m.total_energy_mj:>12.2f}  "
             f"{m.deadline_misses:>8}  "
             f"{m.frequency_requests:>13}")

.. note::

   ``total_energy_mj`` is 0.0 unless ``engine.enable_energy_tracking(True)``
   is called before ``platform.finalize()``. The helper function above handles
   this automatically when ``dvfs_policy`` is not ``None``.

Collecting Results
------------------

For larger sweeps it is convenient to accumulate results in a list of dicts
and format the table once at the end.

.. code-block:: python

   rows = []

   seeds  = range(5)       # 5 independent runs per configuration
   utils  = [0.4] * 8      # 8 tasks, total U = 3.2

   for seed in seeds:
       for dvfs in (None, "ffa", "csf"):
           m = run_simulation(utils, reclamation="grub",
                              dvfs_policy=dvfs, seed=seed)
           rows.append({
               "seed":        seed,
               "dvfs":        dvfs or "none",
               "misses":      m.deadline_misses,
               "energy_mj":   m.total_energy_mj,
               "preemptions": m.preemptions,
           })

   # Print a summary table
   header = f"{'seed':>5}  {'dvfs':>6}  {'misses':>7}  {'energy_mJ':>10}  {'preempts':>9}"
   print(header)
   print("-" * len(header))
   for r in rows:
       print(f"{r['seed']:>5}  {r['dvfs']:>6}  {r['misses']:>7}  "
             f"{r['energy_mj']:>10.2f}  {r['preemptions']:>9}")

Complete Example
----------------

A self-contained utilization sweep that compares GRUB with FFA and CSF,
printing a formatted table for each total utilization level.

.. code-block:: python

   import pyschedsim as sim

   def run_simulation(utilizations, reclamation="grub", dvfs_policy=None,
                      cooldown=0.001, cstate=1, seed=42):
       engine   = sim.Engine()
       platform = engine.platform
       cpu   = platform.add_processor_type("cpu", 1.0)
       clock = platform.add_clock_domain(500.0, 2000.0)
       power = platform.add_power_domain([
           (0, 0, 0.0, 100.0), (1, 0, 0.001, 10.0)
       ])
       for _ in range(4):
           platform.add_processor(cpu, clock, power)

       scenario = sim.from_utilizations(utilizations, seed=seed)
       tasks    = sim.inject_scenario(engine, scenario)

       if dvfs_policy is not None:
           engine.enable_energy_tracking(True)

       platform.finalize()

       for i, tp in enumerate(scenario.tasks):
           sim.schedule_arrivals(engine, tasks[i], tp.jobs)

       procs     = sim.get_all_processors(engine)
       scheduler = sim.EdfScheduler(engine, procs)
       for task in tasks:
           scheduler.add_server(task)

       if reclamation == "grub":
           scheduler.enable_grub()
       else:
           scheduler.enable_cash()

       if dvfs_policy == "pa":
           scheduler.enable_power_aware_dvfs(cooldown)
           scheduler.enable_basic_dpm(cstate)
       elif dvfs_policy == "ffa":
           scheduler.enable_ffa(cooldown, cstate)
       elif dvfs_policy == "csf":
           scheduler.enable_csf(cooldown, cstate)

       allocator = sim.SingleSchedulerAllocator(engine, scheduler)
       writer    = sim.MemoryTraceWriter()
       engine.set_trace_writer(writer)
       engine.run()
       return writer.compute_metrics()


   total_utils = [1.0, 2.0, 3.0, 3.5, 3.8, 3.9]
   n_tasks     = 8
   dvfs_labels = [("GRUB",     None),
                  ("GRUB+FFA", "ffa"),
                  ("GRUB+CSF", "csf")]

   # Header
   col = 12
   header = f"{'Total U':>8}" + "".join(
       f"  {lbl+' misses':>{col}}  {lbl+' mJ':>{col}}"
       for lbl, _ in dvfs_labels
   )
   print(header)
   print("-" * len(header))

   for total_u in total_utils:
       per_task_u = total_u / n_tasks
       utils = [per_task_u] * n_tasks
       row = f"{total_u:>8.2f}"
       for label, dvfs in dvfs_labels:
           m = run_simulation(utils, dvfs_policy=dvfs, seed=0)
           miss_rate = (m.deadline_misses / m.total_jobs
                        if m.total_jobs > 0 else 0.0)
           row += f"  {miss_rate:>{col}.2%}  {m.total_energy_mj:>{col}.1f}"
       print(row)

Next Steps
----------

- :doc:`python-visualization` --- plot sweep results with matplotlib
- :doc:`multi-cluster` --- heterogeneous big.LITTLE platforms
- :doc:`dvfs-energy` --- DVFS and DPM policy details
