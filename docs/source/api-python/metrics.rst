Metrics and Analysis
====================

The metrics API computes aggregate scheduling statistics from simulation
traces. Metrics can be obtained either directly from a
:py:class:`MemoryTraceWriter` after a run, or computed from a previously
saved JSON trace file.

For the equivalent C++ types, see :doc:`../api-cpp/io`. For an end-to-end
walkthrough, see :doc:`../tutorials/analysis`.

SimulationMetrics
-----------------

.. py:class:: SimulationMetrics

   Aggregate statistics computed from a simulation trace.

   All scalar fields are read-write. Map-based fields (per-processor,
   per-task) are accessed through getter methods.

   Scalar Fields
   ^^^^^^^^^^^^^

   .. list-table::
      :header-rows: 1
      :widths: 30 12 58

      * - Field
        - Type
        - Description
      * - ``total_jobs``
        - ``int``
        - Total number of job arrivals observed in the trace.
      * - ``completed_jobs``
        - ``int``
        - Jobs that finished execution before their deadline (or at all).
      * - ``deadline_misses``
        - ``int``
        - Jobs whose absolute deadline elapsed before they completed.
      * - ``preemptions``
        - ``int``
        - Number of preemption events (running job displaced by another).
      * - ``context_switches``
        - ``int``
        - Number of context switch events across all processors.
      * - ``total_energy_mj``
        - ``float``
        - Total energy consumed by all processors in millijoules.
      * - ``average_utilization``
        - ``float``
        - Mean processor utilization averaged across all processors.
          A value of 1.0 means fully loaded.
      * - ``rejected_arrivals``
        - ``int``
        - Job arrivals rejected by an admission controller.
      * - ``cluster_migrations``
        - ``int``
        - Number of times a task was reassigned between clusters.
      * - ``transitions``
        - ``int``
        - Total DVFS frequency transition events.
      * - ``core_state_requests``
        - ``int``
        - Number of C-state change requests across all processors.
      * - ``frequency_requests``
        - ``int``
        - Number of DVFS frequency change requests.

   Map-Based Getter Methods
   ^^^^^^^^^^^^^^^^^^^^^^^^

   .. py:method:: get_energy_per_processor() -> dict[int, float]

      Return energy consumption per processor.

      :returns: A dictionary mapping processor ID to energy in millijoules.

   .. py:method:: get_utilization_per_processor() -> dict[int, float]

      Return utilization per processor.

      :returns: A dictionary mapping processor ID to utilization in [0, 1].

   .. py:method:: get_response_times_per_task() -> dict[int, list[float]]

      Return all measured response times for each task.

      A response time is the elapsed time from job arrival to job
      completion, in seconds.

      :returns: A dictionary mapping task ID to a list of response times
                (one per completed job instance) in seconds.

   .. py:method:: get_deadline_misses_per_task() -> dict[int, int]

      Return deadline miss count per task.

      :returns: A dictionary mapping task ID to the number of deadline
                misses recorded for that task.

   .. py:method:: get_waiting_times_per_task() -> dict[int, list[float]]

      Return all measured waiting times for each task.

      A waiting time is the duration between job arrival and the first
      time that job begins executing, in seconds.

      :returns: A dictionary mapping task ID to a list of waiting times
                in seconds.

ResponseTimeStats
-----------------

.. py:class:: ResponseTimeStats

   Summary statistics for a list of response times.

   Obtain an instance by calling :py:func:`compute_response_time_stats`.
   All fields are read-write.

   .. list-table::
      :header-rows: 1
      :widths: 25 12 63

      * - Field
        - Type
        - Description
      * - ``min``
        - ``float``
        - Minimum response time in seconds.
      * - ``max``
        - ``float``
        - Maximum response time in seconds.
      * - ``mean``
        - ``float``
        - Arithmetic mean of all response times in seconds.
      * - ``median``
        - ``float``
        - Median (50th percentile) response time in seconds.
      * - ``stddev``
        - ``float``
        - Standard deviation of response times in seconds.
      * - ``percentile_95``
        - ``float``
        - 95th percentile response time in seconds.
      * - ``percentile_99``
        - ``float``
        - 99th percentile response time in seconds.

Time-Series Interval Types
---------------------------

The :py:meth:`MemoryTraceWriter.track_frequency_changes`,
:py:meth:`MemoryTraceWriter.track_core_changes`, and
:py:meth:`MemoryTraceWriter.track_config_changes` methods return lists of
interval objects. Each interval represents a period during which the
hardware configuration was constant.

.. py:class:: FrequencyInterval

   A time interval during which a cluster's operating frequency was
   constant.

   .. list-table::
      :header-rows: 1
      :widths: 25 12 63

      * - Field
        - Type
        - Description
      * - ``start``
        - ``float``
        - Interval start time in seconds (inclusive).
      * - ``stop``
        - ``float``
        - Interval end time in seconds (exclusive).
      * - ``frequency``
        - ``float``
        - Frequency value as recorded in the trace. See the warning in
          :doc:`index` regarding units.
      * - ``cluster_id``
        - ``int``
        - Identifier of the clock domain (cluster) this interval applies to.

.. py:class:: CoreCountInterval

   A time interval during which the number of active cores in a cluster
   was constant.

   .. list-table::
      :header-rows: 1
      :widths: 25 12 63

      * - Field
        - Type
        - Description
      * - ``start``
        - ``float``
        - Interval start time in seconds (inclusive).
      * - ``stop``
        - ``float``
        - Interval end time in seconds (exclusive).
      * - ``active_cores``
        - ``int``
        - Number of cores that were active during this interval.
      * - ``cluster_id``
        - ``int``
        - Identifier of the cluster this interval applies to.

.. py:class:: ConfigInterval

   A time interval during which both the frequency and the active core
   count of a cluster were constant. Combines the information of
   :py:class:`FrequencyInterval` and :py:class:`CoreCountInterval`.

   .. list-table::
      :header-rows: 1
      :widths: 25 12 63

      * - Field
        - Type
        - Description
      * - ``start``
        - ``float``
        - Interval start time in seconds (inclusive).
      * - ``stop``
        - ``float``
        - Interval end time in seconds (exclusive).
      * - ``frequency``
        - ``float``
        - Frequency value as recorded in the trace.
      * - ``active_cores``
        - ``int``
        - Number of cores that were active during this interval.

Standalone Functions
--------------------

.. py:function:: compute_metrics_from_file(path: str) -> SimulationMetrics

   Compute metrics directly from a JSON trace file.

   Reads the file incrementally without loading all events into memory at
   once. Suitable for large trace files produced by
   :py:class:`FileJsonTraceWriter`.

   :param path: Path to a JSON trace file.
   :returns: A :py:class:`SimulationMetrics` summarising the trace.
   :raises LoaderError: If the file cannot be opened or the JSON is invalid.

.. py:function:: compute_response_time_stats(response_times: list[float]) -> ResponseTimeStats

   Compute summary statistics for a list of response times.

   :param response_times: A list of response time values in seconds.
                          Must be non-empty.
   :returns: A :py:class:`ResponseTimeStats` with min, max, mean, median,
             stddev, and percentile values.

Examples
--------

Computing Metrics from a MemoryTraceWriter
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

After an in-process simulation, call :py:meth:`MemoryTraceWriter.compute_metrics`
directly:

.. code-block:: python

   import pyschedsim as sim

   engine = sim.Engine()
   # ... platform setup, task injection, scheduler, allocator ...

   writer = sim.MemoryTraceWriter()
   engine.set_trace_writer(writer)
   engine.run()

   metrics = writer.compute_metrics()

   print(f"Jobs completed : {metrics.completed_jobs}/{metrics.total_jobs}")
   print(f"Deadline misses: {metrics.deadline_misses}")
   print(f"Preemptions    : {metrics.preemptions}")
   print(f"Energy         : {metrics.total_energy_mj:.2f} mJ")
   print(f"Avg utilization: {metrics.average_utilization:.2%}")

   # Per-processor breakdown
   for proc_id, util in metrics.get_utilization_per_processor().items():
       energy = metrics.get_energy_per_processor().get(proc_id, 0.0)
       print(f"  Proc {proc_id}: util={util:.2%}  energy={energy:.2f} mJ")

Computing Metrics from a Trace File
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When the trace was written to disk by a :py:class:`FileJsonTraceWriter`,
compute metrics without loading all events into memory:

.. code-block:: python

   import pyschedsim as sim

   metrics = sim.compute_metrics_from_file("trace.json")

   print(f"Jobs: {metrics.completed_jobs}/{metrics.total_jobs}")
   print(f"Deadline misses: {metrics.deadline_misses}")
   print(f"Energy: {metrics.total_energy_mj:.2f} mJ")
   print(f"Rejected arrivals: {metrics.rejected_arrivals}")
   print(f"DVFS transitions: {metrics.transitions}")

Per-Task Response Time Statistics
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Retrieve per-task response times and compute detailed statistics:

.. code-block:: python

   import pyschedsim as sim

   # ... run simulation and obtain metrics ...
   metrics = writer.compute_metrics()

   rt_map = metrics.get_response_times_per_task()
   dm_map = metrics.get_deadline_misses_per_task()

   for task_id, times in rt_map.items():
       if not times:
           continue
       stats = sim.compute_response_time_stats(times)
       misses = dm_map.get(task_id, 0)
       print(
           f"Task {task_id}: "
           f"min={stats.min*1e3:.3f}ms  "
           f"mean={stats.mean*1e3:.3f}ms  "
           f"max={stats.max*1e3:.3f}ms  "
           f"p95={stats.percentile_95*1e3:.3f}ms  "
           f"p99={stats.percentile_99*1e3:.3f}ms  "
           f"misses={misses}"
       )

Using Time-Series Interval Types
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Analyse how DVFS and DPM configuration changes over simulation time:

.. code-block:: python

   import pyschedsim as sim

   # ... run simulation with MemoryTraceWriter ...

   # Frequency time-series: when did the clock domains change frequency?
   freq_intervals = writer.track_frequency_changes()
   for iv in freq_intervals:
       duration = iv.stop - iv.start
       print(
           f"Cluster {iv.cluster_id}: "
           f"[{iv.start:.6f}, {iv.stop:.6f})  "
           f"{iv.frequency:.0f} MHz  ({duration*1e3:.1f} ms)"
       )

   # Core count time-series: how many cores were active over time?
   core_intervals = writer.track_core_changes()
   for iv in core_intervals:
       print(
           f"Cluster {iv.cluster_id}: "
           f"[{iv.start:.6f}, {iv.stop:.6f})  "
           f"{iv.active_cores} active core(s)"
       )

   # Combined configuration: frequency + core count together
   config_intervals = writer.track_config_changes()
   for iv in config_intervals:
       print(
           f"[{iv.start:.6f}, {iv.stop:.6f})  "
           f"{iv.frequency:.0f} MHz, {iv.active_cores} core(s)"
       )

See Also
--------

- :doc:`../tutorials/analysis` --- complete analysis tutorial with C++ and
  Python examples
- :doc:`tracing` --- trace writer classes and :py:class:`TraceRecord`
- :doc:`../api-cpp/io` --- full C++ metrics and analysis API
- :doc:`../cli/index` --- ``schedview`` command-line metrics tool
