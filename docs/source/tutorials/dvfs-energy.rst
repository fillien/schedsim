DVFS and Energy Management
==========================

Schedsim supports Dynamic Voltage and Frequency Scaling (DVFS) and Dynamic
Power Management (DPM) to simulate energy-aware scheduling. This tutorial
shows how to enable these features and measure energy consumption.

Prerequisites
-------------

DVFS and DPM policies build on top of bandwidth reclamation. You must enable
a reclamation policy (GRUB or CASH) **before** enabling any DVFS or DPM policy.

.. admonition:: Policy ordering

   1. Create the scheduler and add CBS servers
   2. Enable reclamation: ``enable_grub()`` or ``enable_cash()``
   3. Enable DVFS: ``enable_power_aware_dvfs()`` or a combined policy
   4. Enable DPM: ``enable_basic_dpm()`` (or use a combined DVFS+DPM policy)
   5. Create the allocator

The platform must also have appropriate hardware support:

- **DVFS**: ClockDomain with ``freq_min < freq_max``
- **DPM**: PowerDomain with at least two C-state levels (C0 for active, C1+ for sleep)
- **Transition delay**: Set a non-zero ``transition_delay`` on the ClockDomain
  for realistic DVFS modeling (zero delay applies frequency changes instantly)

Available Policies
------------------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Method
     - Description
   * - ``enable_grub()``
     - GRUB bandwidth reclamation (prerequisite for DVFS)
   * - ``enable_cash()``
     - CASH bandwidth reclamation (alternative to GRUB)
   * - ``enable_power_aware_dvfs(cooldown)``
     - Power-Aware DVFS: sets frequency to ``f_max * ((m-1)*u_max + U_total) / m``
   * - ``enable_basic_dpm(cstate)``
     - Puts idle processors into the specified C-state
   * - ``enable_ffa(cooldown, cstate)``
     - FFA: combined DVFS + DPM with frequency-first approach
   * - ``enable_csf(cooldown, cstate)``
     - CSF: combined DVFS + DPM with core-shutdown-first approach
   * - ``enable_ffa_timer(cooldown, cstate)``
     - FFA with timer-based frequency updates
   * - ``enable_csf_timer(cooldown, cstate)``
     - CSF with timer-based frequency updates

The ``cooldown`` parameter is a ``Duration`` (seconds in Python) specifying the
minimum interval between frequency changes. The ``cstate`` parameter is the
target C-state level for idle processors.

C++ Example
-----------

.. code-block:: cpp

   #include <schedsim/core/engine.hpp>
   #include <schedsim/core/platform.hpp>
   #include <schedsim/core/types.hpp>
   #include <schedsim/algo/edf_scheduler.hpp>
   #include <schedsim/algo/single_scheduler_allocator.hpp>
   #include <schedsim/io/platform_loader.hpp>
   #include <schedsim/io/scenario_loader.hpp>
   #include <schedsim/io/scenario_injection.hpp>
   #include <schedsim/io/trace_writers.hpp>

   #include <iostream>
   #include <vector>

   using namespace schedsim;

   int main() {
       core::Engine engine;

       // Load a platform with DVFS support (freq range + transition delay)
       io::load_platform(engine, "platforms/exynos5422LITTLE.json");

       // Load and inject scenario
       auto scenario = io::load_scenario("scenario.json");
       auto tasks = io::inject_scenario(engine, scenario);
       for (std::size_t i = 0; i < scenario.tasks.size(); ++i) {
           io::schedule_arrivals(engine, *tasks[i], scenario.tasks[i].jobs);
       }

       // Enable energy tracking BEFORE finalize
       engine.enable_energy_tracking(true);

       engine.platform().finalize();

       // Create scheduler
       std::vector<core::Processor*> procs;
       for (std::size_t i = 0; i < engine.platform().processor_count(); ++i) {
           procs.push_back(&engine.platform().processor(i));
       }
       algo::EdfScheduler scheduler(engine, procs);

       for (auto* task : tasks) {
           scheduler.add_server(*task);
       }

       // Enable policies in order: reclamation, then DVFS, then DPM
       scheduler.enable_grub();
       scheduler.enable_power_aware_dvfs(core::duration_from_seconds(0.001));
       scheduler.enable_basic_dpm(1);  // target C-state 1

       algo::SingleSchedulerAllocator allocator(engine, scheduler);

       // Run with tracing
       io::MemoryTraceWriter trace_writer;
       engine.set_trace_writer(&trace_writer);
       engine.run();

       // Query energy consumption
       std::cout << "Total energy: " << engine.total_energy().mj << " mJ\n";

       for (std::size_t i = 0; i < engine.platform().processor_count(); ++i) {
           std::cout << "  Processor " << i << ": "
                     << engine.processor_energy(i).mj << " mJ\n";
       }

       return 0;
   }

Python Example
--------------

.. code-block:: python

   import pyschedsim as sim

   engine = sim.Engine()

   # Load platform with DVFS support
   sim.load_platform(engine, "platforms/exynos5422LITTLE.json")

   # Load and inject scenario
   scenario = sim.load_scenario("scenario.json")
   tasks = sim.inject_scenario(engine, scenario)
   for i, tp in enumerate(scenario.tasks):
       sim.schedule_arrivals(engine, tasks[i], tp.jobs)

   # Enable energy tracking
   engine.enable_energy_tracking(True)

   engine.platform.finalize()

   # Create scheduler
   procs = sim.get_all_processors(engine)
   scheduler = sim.EdfScheduler(engine, procs)
   for task in tasks:
       scheduler.add_server(task)

   # Enable GRUB reclamation + Power-Aware DVFS + DPM
   scheduler.enable_grub()
   scheduler.enable_power_aware_dvfs(0.001)  # 1ms cooldown
   scheduler.enable_basic_dpm(1)

   allocator = sim.SingleSchedulerAllocator(engine, scheduler)

   writer = sim.MemoryTraceWriter()
   engine.set_trace_writer(writer)
   engine.run()

   # Query energy
   print(f"Total energy: {engine.total_energy():.2f} mJ")

   for i in range(engine.platform.processor_count):
       print(f"  Processor {i}: {engine.processor_energy(i):.2f} mJ")

Combined DVFS+DPM Policies
---------------------------

Instead of enabling DVFS and DPM separately, you can use a combined policy
that coordinates both. FFA (Frequency-First Approach) reduces frequency before
shutting down cores; CSF (Core-Shutdown First) does the opposite.

.. code-block:: cpp

   // FFA: reduce frequency first, then shut down cores
   scheduler.enable_grub();
   scheduler.enable_ffa(core::duration_from_seconds(0.001), 1);

   // CSF: shut down cores first, then reduce frequency
   scheduler.enable_grub();
   scheduler.enable_csf(core::duration_from_seconds(0.001), 1);

   // Timer variants re-evaluate periodically instead of on every event
   scheduler.enable_grub();
   scheduler.enable_ffa_timer(core::duration_from_seconds(0.001), 1);

.. code-block:: python

   # FFA in Python
   scheduler.enable_grub()
   scheduler.enable_ffa(0.001, 1)

   # CSF in Python
   scheduler.enable_grub()
   scheduler.enable_csf(0.001, 1)

.. note::

   Combined policies (FFA, CSF and their timer variants) include both DVFS
   and DPM. Do not additionally call ``enable_power_aware_dvfs()`` or
   ``enable_basic_dpm()`` when using them.

Building a DVFS-Capable Platform from Scratch
----------------------------------------------

If you're constructing the platform programmatically rather than loading it
from JSON, ensure the clock domain has a frequency range and the power domain
has multiple C-states:

.. code-block:: cpp

   auto& cpu = platform.add_processor_type("A7", 0.5);

   // Frequency range with 100us transition delay
   auto& clock = platform.add_clock_domain(
       core::Frequency{500.0},   // freq_min (MHz)
       core::Frequency{1400.0},  // freq_max (MHz)
       core::duration_from_seconds(0.0001)  // transition_delay
   );

   // Two C-states: active (C0) and sleep (C1)
   auto& power = platform.add_power_domain({
       {0, core::CStateScope::PerProcessor,
        core::duration_from_seconds(0.0), core::Power{200.0}},
       {1, core::CStateScope::PerProcessor,
        core::duration_from_seconds(0.001), core::Power{5.0}},
   });

.. code-block:: python

   cpu = platform.add_processor_type("A7", 0.5)
   clock = platform.add_clock_domain(500.0, 1400.0, 0.0001)  # transition_delay=100us
   power = platform.add_power_domain([
       (0, 0, 0.0, 200.0),     # C0: active, 200 mW
       (1, 0, 0.001, 5.0),     # C1: sleep, 5 mW, 1ms wake latency
   ])

Next Steps
----------

- :doc:`multi-cluster` --- heterogeneous platforms with per-cluster DVFS
- :doc:`analysis` --- extract energy metrics and frequency time-series from traces
