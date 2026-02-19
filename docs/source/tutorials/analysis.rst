Trace Analysis and Metrics
==========================

After running a simulation, Schedsim can compute scheduling metrics from the
trace output. This tutorial covers the ``schedview`` CLI tool, the C++ metrics
API, and the Python equivalents.

Using ``schedview``
-------------------

The simplest way to analyze a trace is the ``schedview`` command-line tool:

.. code-block:: bash

   # Basic metrics: job counts, deadline misses, preemptions
   schedview trace.json

   # Include energy breakdown
   schedview trace.json --energy

   # Include per-task response time statistics
   schedview trace.json --response-times

   # Full analysis
   schedview trace.json --energy --response-times

File-Based Analysis
--------------------

For programmatic access without running a simulation, load metrics directly
from a trace file:

C++ Example
^^^^^^^^^^^

.. code-block:: cpp

   #include <schedsim/io/metrics.hpp>
   #include <iostream>

   using namespace schedsim;

   int main() {
       auto metrics = io::compute_metrics_from_file("trace.json");

       std::cout << "Jobs: " << metrics.completed_jobs
                 << "/" << metrics.total_jobs << "\n";
       std::cout << "Deadline misses: " << metrics.deadline_misses << "\n";
       std::cout << "Preemptions: " << metrics.preemptions << "\n";
       std::cout << "Energy: " << metrics.total_energy_mj << " mJ\n";

       return 0;
   }

Python Example
^^^^^^^^^^^^^^

.. code-block:: python

   import pyschedsim as sim

   metrics = sim.compute_metrics_from_file("trace.json")
   print(f"Jobs: {metrics.completed_jobs}/{metrics.total_jobs}")
   print(f"Deadline misses: {metrics.deadline_misses}")
   print(f"Energy: {metrics.total_energy_mj:.2f} mJ")

In-Memory Analysis
------------------

When running simulations programmatically, use a ``MemoryTraceWriter`` to
compute metrics without writing to disk:

C++ Example
^^^^^^^^^^^

.. code-block:: cpp

   #include <schedsim/io/metrics.hpp>
   #include <schedsim/io/trace_writers.hpp>

   // ... set up engine, scheduler, etc. ...

   io::MemoryTraceWriter writer;
   engine.set_trace_writer(&writer);
   engine.run();

   auto metrics = io::compute_metrics(writer.records());

Python Example
^^^^^^^^^^^^^^

.. code-block:: python

   writer = sim.MemoryTraceWriter()
   engine.set_trace_writer(writer)
   engine.run()

   metrics = writer.compute_metrics()

SimulationMetrics Fields
^^^^^^^^^^^^^^^^^^^^^^^^

The ``SimulationMetrics`` struct contains:

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Field
     - Type
     - Description
   * - ``total_jobs``
     - int
     - Total number of job arrivals
   * - ``completed_jobs``
     - int
     - Jobs that finished execution
   * - ``deadline_misses``
     - int
     - Jobs that missed their absolute deadline
   * - ``preemptions``
     - int
     - Number of preemption events
   * - ``context_switches``
     - int
     - Number of context switch events
   * - ``total_energy_mj``
     - double
     - Total energy consumption in millijoules
   * - ``average_utilization``
     - double
     - Average processor utilization across all processors
   * - ``rejected_tasks``
     - int
     - Tasks rejected by admission control
   * - ``cluster_migrations``
     - int
     - Tasks moved between clusters
   * - ``transitions``
     - int
     - Frequency transition events
   * - ``core_state_requests``
     - int
     - C-state change requests
   * - ``frequency_requests``
     - int
     - DVFS frequency change requests

Map-based fields are accessed through getter methods:

.. code-block:: python

   # Per-processor utilization: {proc_id: utilization}
   util_map = metrics.get_utilization_per_processor()

   # Per-processor energy: {proc_id: energy_mJ}
   energy_map = metrics.get_energy_per_processor()

   # Per-task response times: {task_id: [rt1, rt2, ...]}
   rt_map = metrics.get_response_times_per_task()

   # Per-task deadline misses: {task_id: count}
   dm_map = metrics.get_deadline_misses_per_task()

   # Per-task waiting times: {task_id: [wt1, wt2, ...]}
   wt_map = metrics.get_waiting_times_per_task()

.. code-block:: cpp

   // C++: direct field access
   for (const auto& [proc_id, util] : metrics.utilization_per_processor) {
       std::cout << "Proc " << proc_id << ": " << util << "\n";
   }

Per-Task Response Time Statistics
---------------------------------

Compute summary statistics for each task's response times:

.. code-block:: cpp

   for (const auto& [task_id, times] : metrics.response_times_per_task) {
       auto stats = io::compute_response_time_stats(times);
       std::cout << "Task " << task_id
                 << " min=" << stats.min
                 << " max=" << stats.max
                 << " mean=" << stats.mean
                 << " p95=" << stats.percentile_95
                 << " p99=" << stats.percentile_99 << "\n";
   }

.. code-block:: python

   rt_map = metrics.get_response_times_per_task()
   for task_id, times in rt_map.items():
       stats = sim.compute_response_time_stats(times)
       print(f"Task {task_id}: "
             f"min={stats.min:.6f} max={stats.max:.6f} "
             f"mean={stats.mean:.6f} p95={stats.percentile_95:.6f}")

The ``ResponseTimeStats`` struct contains: ``min``, ``max``, ``mean``,
``median``, ``stddev``, ``percentile_95``, ``percentile_99``.

Time-Series Analysis
--------------------

Track how hardware configuration changes over time. These functions return
lists of intervals, each with a start time, stop time, and the configuration
value during that interval.

Frequency Changes
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

   auto intervals = io::track_frequency_changes(writer.records());
   for (const auto& iv : intervals) {
       std::cout << "[" << iv.start << ", " << iv.stop << ") "
                 << "cluster=" << iv.cluster_id
                 << " freq=" << iv.frequency << " MHz\n";
   }

.. code-block:: python

   intervals = writer.track_frequency_changes()
   for iv in intervals:
       print(f"[{iv.start:.6f}, {iv.stop:.6f}) "
             f"cluster={iv.cluster_id} freq={iv.frequency:.0f} MHz")

Active Core Count
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

   auto intervals = io::track_core_changes(writer.records());
   for (const auto& iv : intervals) {
       std::cout << "[" << iv.start << ", " << iv.stop << ") "
                 << "cluster=" << iv.cluster_id
                 << " active=" << iv.active_cores << "\n";
   }

.. code-block:: python

   intervals = writer.track_core_changes()
   for iv in intervals:
       print(f"[{iv.start:.6f}, {iv.stop:.6f}) "
             f"cluster={iv.cluster_id} active={iv.active_cores}")

Combined Configuration
^^^^^^^^^^^^^^^^^^^^^^

``track_config_changes()`` returns intervals with both frequency and active
core count:

.. code-block:: python

   intervals = writer.track_config_changes()
   for iv in intervals:
       print(f"[{iv.start:.6f}, {iv.stop:.6f}) "
             f"freq={iv.frequency:.0f} MHz, cores={iv.active_cores}")

Complete Analysis Example
-------------------------

Putting it all together --- run a simulation and extract all metrics:

.. code-block:: python

   import pyschedsim as sim

   # Set up and run simulation
   engine = sim.Engine()
   sim.load_platform(engine, "platforms/exynos5422LITTLE.json")

   scenario = sim.load_scenario("scenario.json")
   tasks = sim.inject_scenario(engine, scenario)
   for i, tp in enumerate(scenario.tasks):
       sim.schedule_arrivals(engine, tasks[i], tp.jobs)

   engine.enable_energy_tracking(True)
   engine.platform.finalize()

   procs = sim.get_all_processors(engine)
   scheduler = sim.EdfScheduler(engine, procs)
   for task in tasks:
       scheduler.add_server(task)
   scheduler.enable_grub()

   allocator = sim.SingleSchedulerAllocator(engine, scheduler)

   writer = sim.MemoryTraceWriter()
   engine.set_trace_writer(writer)
   engine.run()

   # --- Metrics ---
   metrics = writer.compute_metrics()
   print(f"Completed: {metrics.completed_jobs}/{metrics.total_jobs}")
   print(f"Deadline misses: {metrics.deadline_misses}")
   print(f"Energy: {metrics.total_energy_mj:.2f} mJ")
   print(f"Preemptions: {metrics.preemptions}")

   # Per-processor utilization
   for proc_id, util in metrics.get_utilization_per_processor().items():
       print(f"  Proc {proc_id}: {util:.2%} utilization")

   # Per-task response times
   for task_id, times in metrics.get_response_times_per_task().items():
       stats = sim.compute_response_time_stats(times)
       print(f"  Task {task_id}: mean RT = {stats.mean:.6f}s, "
             f"max = {stats.max:.6f}s")

   # Frequency time-series
   for iv in writer.track_frequency_changes():
       print(f"  [{iv.start:.6f}, {iv.stop:.6f}) "
             f"cluster {iv.cluster_id}: {iv.frequency:.0f} MHz")

Next Steps
----------

- :doc:`first-simulation` --- if you haven't built your first simulation yet
- :doc:`dvfs-energy` --- enable DVFS/DPM to generate energy-related traces
- :doc:`multi-cluster` --- heterogeneous platforms with cluster migrations
