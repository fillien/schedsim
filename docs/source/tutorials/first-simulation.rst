Your First Simulation
=====================

This tutorial walks through building and running a complete EDF simulation
from scratch, both in C++ and Python.

Overview
--------

A Schedsim simulation requires four components:

1. An **Engine** that drives the event loop
2. A **Platform** describing the hardware (processors, clock domains, power domains)
3. **Tasks** with job arrivals
4. A **Scheduler** and **Allocator** that assign jobs to processors

The setup follows a strict ordering:

.. admonition:: Ordering constraints

   1. Add tasks and processors to the platform **before** calling ``finalize()``
   2. Call ``platform.finalize()`` before creating schedulers
   3. Create the scheduler and add CBS servers **after** finalize
   4. Create the allocator **after** the scheduler (it installs the job arrival handler)
   5. Set the trace writer any time before ``engine.run()``

C++ Example
-----------

.. code-block:: cpp

   #include <schedsim/core/engine.hpp>
   #include <schedsim/core/platform.hpp>
   #include <schedsim/core/types.hpp>
   #include <schedsim/algo/edf_scheduler.hpp>
   #include <schedsim/algo/cbs_server.hpp>
   #include <schedsim/algo/single_scheduler_allocator.hpp>
   #include <schedsim/io/trace_writers.hpp>

   #include <iostream>
   #include <vector>

   using namespace schedsim;

   int main() {
       // 1. Create the simulation engine
       core::Engine engine;

       // 2. Build the platform
       auto& platform = engine.platform();

       // Processor type: name and relative performance (1.0 = reference)
       auto& cpu = platform.add_processor_type("cpu", 1.0);

       // Clock domain: frequency range in MHz
       auto& clock = platform.add_clock_domain(
           core::Frequency{500.0},   // freq_min
           core::Frequency{2000.0}   // freq_max
       );

       // Power domain: list of C-state levels
       // Each level: {level, scope, wake_latency, idle_power}
       auto& power = platform.add_power_domain({
           {0, core::CStateScope::PerProcessor,
            core::duration_from_seconds(0.0), core::Power{100.0}},  // C0 (active)
           {1, core::CStateScope::PerProcessor,
            core::duration_from_seconds(0.001), core::Power{10.0}}, // C1 (light sleep)
       });

       // Add two processors
       platform.add_processor(cpu, clock, power);
       platform.add_processor(cpu, clock, power);

       // Add two periodic tasks: period, relative_deadline, wcet (in seconds)
       auto& task1 = platform.add_task(
           core::duration_from_seconds(0.010),  // 10 ms period
           core::duration_from_seconds(0.010),  // implicit deadline
           core::duration_from_seconds(0.003)   // 3 ms WCET
       );
       auto& task2 = platform.add_task(
           core::duration_from_seconds(0.020),  // 20 ms period
           core::duration_from_seconds(0.020),
           core::duration_from_seconds(0.005)   // 5 ms WCET
       );

       // 3. Schedule job arrivals (before or after finalize)
       // Two jobs for task1, arriving at t=0 and t=10ms
       engine.schedule_job_arrival(task1,
           core::time_from_seconds(0.0), core::duration_from_seconds(0.002));
       engine.schedule_job_arrival(task1,
           core::time_from_seconds(0.010), core::duration_from_seconds(0.003));

       // One job for task2, arriving at t=0
       engine.schedule_job_arrival(task2,
           core::time_from_seconds(0.0), core::duration_from_seconds(0.004));

       // 4. Finalize the platform (locks the hardware configuration)
       platform.finalize();

       // 5. Create the EDF scheduler with all processors
       std::vector<core::Processor*> procs;
       for (std::size_t i = 0; i < platform.processor_count(); ++i) {
           procs.push_back(&platform.processor(i));
       }
       algo::EdfScheduler scheduler(engine, procs);

       // 6. Add a CBS server for each task (uses task's own period/WCET as budget)
       scheduler.add_server(task1);
       scheduler.add_server(task2);

       // 7. Create the allocator (connects job arrivals to the scheduler)
       algo::SingleSchedulerAllocator allocator(engine, scheduler);

       // 8. Attach a trace writer to see what happens
       io::JsonTraceWriter trace_writer(std::cout);
       engine.set_trace_writer(&trace_writer);

       // 9. Run the simulation
       engine.run();
       trace_writer.finalize();

       return 0;
   }

Python Example
--------------

The Python bindings expose the same API through the ``pyschedsim`` module.
Duration, TimePoint, and Frequency values are automatically converted to/from
Python floats (seconds for time, MHz for frequency, mW for power).

.. code-block:: python

   import pyschedsim as sim

   # 1. Create the simulation engine
   engine = sim.Engine()

   # 2. Build the platform
   platform = engine.platform

   cpu = platform.add_processor_type("cpu", 1.0)
   clock = platform.add_clock_domain(500.0, 2000.0)  # freq_min, freq_max (MHz)

   # C-states: list of (level, scope, wake_latency_s, power_mW) tuples
   # scope: 0 = PerProcessor, 1 = DomainWide
   power = platform.add_power_domain([
       (0, 0, 0.0, 100.0),    # C0: active
       (1, 0, 0.001, 10.0),   # C1: light sleep
   ])

   proc0 = platform.add_processor(cpu, clock, power)
   proc1 = platform.add_processor(cpu, clock, power)

   # Tasks: period, relative_deadline, wcet (all in seconds)
   task1 = platform.add_task(0.010, 0.010, 0.003)
   task2 = platform.add_task(0.020, 0.020, 0.005)

   # 3. Schedule job arrivals
   engine.schedule_job_arrival(task1, 0.0, 0.002)   # arrival_time, exec_time
   engine.schedule_job_arrival(task1, 0.010, 0.003)
   engine.schedule_job_arrival(task2, 0.0, 0.004)

   # 4. Finalize the platform
   platform.finalize()

   # 5. Create the scheduler
   procs = sim.get_all_processors(engine)
   scheduler = sim.EdfScheduler(engine, procs)

   # 6. Add CBS servers
   scheduler.add_server(task1)
   scheduler.add_server(task2)

   # 7. Create the allocator
   allocator = sim.SingleSchedulerAllocator(engine, scheduler)

   # 8. Attach a trace writer
   writer = sim.MemoryTraceWriter()
   engine.set_trace_writer(writer)

   # 9. Run
   engine.run()

   # 10. Inspect results
   print(f"Recorded {writer.record_count} trace events")
   for i in range(writer.record_count):
       rec = writer.record(i)
       print(f"  t={rec.time:.6f}  {rec.type}")

What Each Step Does
-------------------

**Engine**
   The event-driven simulation loop. It maintains a priority queue of timed
   events and advances simulation time by dispatching them in order.

**Platform**
   A container for all hardware resources. ``finalize()`` locks the
   configuration and computes derived values like the reference performance.

**Processor type, clock domain, power domain**
   These describe the hardware characteristics. A processor is the combination
   of one type, one clock domain, and one power domain.

**Task**
   A periodic real-time task with a worst-case execution time (WCET), period,
   and relative deadline.

**EdfScheduler**
   An Earliest Deadline First scheduler. It dispatches CBS servers to
   processors based on their absolute deadlines.

**CBS server**
   A Constant Bandwidth Server wrapping each task. It enforces a
   budget/period utilization bound and postpones deadlines when the budget is
   exhausted. By default, ``add_server(task)`` uses the task's own WCET and
   period.

**SingleSchedulerAllocator**
   Routes incoming job arrivals to the single scheduler. For multi-cluster
   setups, see :doc:`multi-cluster`.

**Trace writer**
   Records simulation events. ``JsonTraceWriter`` streams JSON to an output
   stream; ``MemoryTraceWriter`` buffers events in memory for post-processing.

Next Steps
----------

- :doc:`scenarios` --- load task sets from JSON instead of hardcoding them
- :doc:`dvfs-energy` --- add DVFS frequency scaling and energy tracking
- :doc:`analysis` --- compute metrics from simulation traces
