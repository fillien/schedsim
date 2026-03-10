Python Quick Start
==================

This tutorial builds a complete scheduling simulation from scratch in Python,
without requiring any external configuration files. By the end you will have
a working two-processor EDF simulation with CBS servers and bandwidth
reclamation, and you will know how to inspect response times and processor
utilization.

.. admonition:: Ordering constraints

   1. Add tasks and processors to the platform **before** calling ``finalize()``
   2. Call ``platform.finalize()`` before creating the scheduler
   3. Add CBS servers and enable policies **after** creating the scheduler
   4. Create the allocator **after** the scheduler
   5. Set the trace writer any time before ``engine.run()``

Creating the Simulation Engine
-------------------------------

The ``Engine`` owns the event-driven simulation loop. All other objects hold a
reference to it.

.. code-block:: python

   import pyschedsim as sim

   engine = sim.Engine()

Building the Platform
---------------------

The platform describes the hardware. It holds processor types, clock domains,
power domains, and the actual processor instances.

.. code-block:: python

   platform = engine.platform

   # Processor type: name and relative performance (1.0 = reference speed)
   cpu = platform.add_processor_type("cpu", 1.0)

   # Clock domain: frequency range in MHz (enables DVFS when min != max)
   clock = platform.add_clock_domain(500.0, 2000.0)

   # Power domain: list of C-state levels
   # Each tuple: (level, scope, wake_latency_s, idle_power_mW)
   # scope 0 = PerProcessor, scope 1 = DomainWide
   power = platform.add_power_domain([
       (0, 0, 0.0,   100.0),   # C0: active
       (1, 0, 0.001,  10.0),   # C1: light sleep, 1 ms wake latency
   ])

   # Two processors sharing the same clock and power domain
   proc0 = platform.add_processor(cpu, clock, power)
   proc1 = platform.add_processor(cpu, clock, power)

Defining Tasks
--------------

Each task has a period, a relative deadline, and a worst-case execution time
(WCET), all in seconds. ``add_task()`` assigns IDs automatically starting at 1.

.. code-block:: python

   # period, relative_deadline, wcet  (all in seconds)
   task1 = platform.add_task(0.010, 0.010, 0.003)   # 10 ms / 10 ms / 3 ms
   task2 = platform.add_task(0.020, 0.020, 0.006)   # 20 ms / 20 ms / 6 ms
   task3 = platform.add_task(0.040, 0.040, 0.008)   # 40 ms / 40 ms / 8 ms

The three tasks have a total utilization of 0.3 + 0.3 + 0.2 = 0.8 per
processor, comfortably below the two-processor capacity of 2.0.

Scheduling Job Arrivals
------------------------

Each job arrival is a discrete event with an arrival time and an actual
execution time. The actual execution time must not exceed the task's WCET.

.. code-block:: python

   # Task 1: three jobs in the first 30 ms
   engine.schedule_job_arrival(task1, 0.000, 0.002)
   engine.schedule_job_arrival(task1, 0.010, 0.003)
   engine.schedule_job_arrival(task1, 0.020, 0.001)

   # Task 2: two jobs in the first 40 ms
   engine.schedule_job_arrival(task2, 0.000, 0.005)
   engine.schedule_job_arrival(task2, 0.020, 0.004)

   # Task 3: two jobs, one early and one late
   engine.schedule_job_arrival(task3, 0.000, 0.007)
   engine.schedule_job_arrival(task3, 0.040, 0.006)

Setting Up the Scheduler
------------------------

``platform.finalize()`` locks the hardware configuration and must be called
before creating the scheduler. ``get_all_processors()`` is a convenience
function that returns every processor in the platform as a list.

.. code-block:: python

   # Lock the platform configuration
   platform.finalize()

   # Create an EDF scheduler over both processors
   procs = sim.get_all_processors(engine)
   scheduler = sim.EdfScheduler(engine, procs)

   # Wrap each task in a CBS server
   # add_server(task) uses the task's own WCET and period as budget/period
   scheduler.add_server(task1)
   scheduler.add_server(task2)
   scheduler.add_server(task3)

   # The allocator routes job arrivals to the scheduler
   allocator = sim.SingleSchedulerAllocator(engine, scheduler)

Enabling Bandwidth Reclamation
-------------------------------

GRUB (Greedy Reclamation of Unused Bandwidth) allows CBS servers to consume
spare processor capacity left by other tasks. When a job finishes early, its
unused budget is redistributed rather than wasted.

.. code-block:: python

   # Enable GRUB (call after add_server, before creating the allocator
   # is also acceptable -- but always before engine.run())
   scheduler.enable_grub()

.. note::

   GRUB must be enabled **before** ``engine.run()`` is called. Enabling it
   after the simulation has started has no effect.

Running and Collecting Results
-------------------------------

Attach a ``MemoryTraceWriter`` to capture all events in memory, then call
``engine.run()`` to process every queued job arrival.

.. code-block:: python

   writer = sim.MemoryTraceWriter()
   engine.set_trace_writer(writer)

   engine.run()

   # Compute metrics from the buffered trace
   metrics = writer.compute_metrics()

   print(f"Jobs:            {metrics.completed_jobs}/{metrics.total_jobs}")
   print(f"Deadline misses: {metrics.deadline_misses}")
   print(f"Preemptions:     {metrics.preemptions}")
   print(f"Context switches:{metrics.context_switches}")

Inspecting Detailed Results
----------------------------

Per-task response times and per-processor utilization are available through
getter methods on the metrics object.

.. code-block:: python

   # Per-task response time statistics
   rt_map = metrics.get_response_times_per_task()
   for task_id, times in rt_map.items():
       stats = sim.compute_response_time_stats(times)
       print(f"Task {task_id}: "
             f"mean={stats.mean*1e3:.2f} ms  "
             f"max={stats.max*1e3:.2f} ms  "
             f"p95={stats.percentile_95*1e3:.2f} ms")

   # Per-processor utilization
   util_map = metrics.get_utilization_per_processor()
   for proc_id, util in util_map.items():
       print(f"Processor {proc_id}: {util:.1%} utilization")

``compute_response_time_stats()`` accepts any list of floats (seconds) and
returns a ``ResponseTimeStats`` object with fields ``min``, ``max``, ``mean``,
``median``, ``stddev``, ``percentile_95``, and ``percentile_99``.

Complete Example
----------------

The following self-contained script combines every step above. Copy it into a
file and run it directly with ``python example.py``.

.. code-block:: python

   import pyschedsim as sim

   # ── Engine ────────────────────────────────────────────────────────────────
   engine = sim.Engine()
   platform = engine.platform

   # ── Platform ──────────────────────────────────────────────────────────────
   cpu   = platform.add_processor_type("cpu", 1.0)
   clock = platform.add_clock_domain(500.0, 2000.0)
   power = platform.add_power_domain([
       (0, 0, 0.0,   100.0),
       (1, 0, 0.001,  10.0),
   ])
   proc0 = platform.add_processor(cpu, clock, power)
   proc1 = platform.add_processor(cpu, clock, power)

   # ── Tasks ─────────────────────────────────────────────────────────────────
   task1 = platform.add_task(0.010, 0.010, 0.003)
   task2 = platform.add_task(0.020, 0.020, 0.006)
   task3 = platform.add_task(0.040, 0.040, 0.008)

   # ── Job arrivals ──────────────────────────────────────────────────────────
   engine.schedule_job_arrival(task1, 0.000, 0.002)
   engine.schedule_job_arrival(task1, 0.010, 0.003)
   engine.schedule_job_arrival(task1, 0.020, 0.001)
   engine.schedule_job_arrival(task2, 0.000, 0.005)
   engine.schedule_job_arrival(task2, 0.020, 0.004)
   engine.schedule_job_arrival(task3, 0.000, 0.007)
   engine.schedule_job_arrival(task3, 0.040, 0.006)

   # ── Scheduler ─────────────────────────────────────────────────────────────
   platform.finalize()

   procs     = sim.get_all_processors(engine)
   scheduler = sim.EdfScheduler(engine, procs)
   scheduler.add_server(task1)
   scheduler.add_server(task2)
   scheduler.add_server(task3)
   scheduler.enable_grub()

   allocator = sim.SingleSchedulerAllocator(engine, scheduler)

   # ── Trace + run ───────────────────────────────────────────────────────────
   writer = sim.MemoryTraceWriter()
   engine.set_trace_writer(writer)
   engine.run()

   # ── Results ───────────────────────────────────────────────────────────────
   metrics = writer.compute_metrics()
   print(f"Jobs:            {metrics.completed_jobs}/{metrics.total_jobs}")
   print(f"Deadline misses: {metrics.deadline_misses}")
   print(f"Preemptions:     {metrics.preemptions}")

   rt_map = metrics.get_response_times_per_task()
   for task_id, times in rt_map.items():
       stats = sim.compute_response_time_stats(times)
       print(f"Task {task_id}: mean RT = {stats.mean*1e3:.2f} ms, "
             f"max RT = {stats.max*1e3:.2f} ms")

   for proc_id, util in metrics.get_utilization_per_processor().items():
       print(f"Processor {proc_id}: {util:.1%}")

Next Steps
----------

- :doc:`python-batch` --- run parameter sweeps and compare scheduling policies
- :doc:`dvfs-energy` --- add DVFS frequency scaling and energy tracking
- :doc:`analysis` --- extract time-series metrics from simulation traces
- :doc:`python-visualization` --- plot results with matplotlib
