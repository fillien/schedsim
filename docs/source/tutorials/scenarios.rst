Generating and Loading Scenarios
================================

Instead of hardcoding tasks and job arrivals, Schedsim can load workloads
from JSON scenario files or generate them programmatically.

Scenario JSON Format
--------------------

A scenario file describes tasks and their job arrival sequences:

.. code-block:: json

   {
     "tasks": [
       {
         "id": 1,
         "period": 0.010,
         "relative_deadline": 0.010,
         "wcet": 0.003,
         "jobs": [
           {"arrival": 0.000, "duration": 0.002},
           {"arrival": 0.010, "duration": 0.003},
           {"arrival": 0.020, "duration": 0.001}
         ]
       },
       {
         "id": 2,
         "period": 0.020,
         "relative_deadline": 0.020,
         "wcet": 0.005,
         "jobs": [
           {"arrival": 0.000, "duration": 0.004},
           {"arrival": 0.020, "duration": 0.005}
         ]
       }
     ]
   }

All time values are in seconds. The ``duration`` of each job is the actual
execution time for that instance (must not exceed ``wcet``). Task IDs are
1-based and preserved through loading and injection.

Loading and Injecting Scenarios
-------------------------------

Loading a scenario is a three-step process:

1. **Load** the JSON into a ``ScenarioData`` struct
2. **Inject** the scenario into the engine (creates ``Task`` objects in the platform)
3. **Schedule** job arrivals from the scenario data

.. admonition:: Ordering constraints

   - ``inject_scenario()`` must be called **before** ``platform.finalize()``
     (it adds tasks to the platform)
   - ``schedule_arrivals()`` can be called before or after finalize
   - Create the scheduler **after** finalize

C++ Example
^^^^^^^^^^^

.. code-block:: cpp

   #include <schedsim/core/engine.hpp>
   #include <schedsim/io/platform_loader.hpp>
   #include <schedsim/io/scenario_loader.hpp>
   #include <schedsim/io/scenario_injection.hpp>
   #include <schedsim/algo/edf_scheduler.hpp>
   #include <schedsim/algo/single_scheduler_allocator.hpp>

   using namespace schedsim;

   int main() {
       core::Engine engine;

       // Load the hardware platform from JSON
       io::load_platform(engine, "platforms/exynos5422LITTLE.json");

       // Load the scenario
       auto scenario = io::load_scenario("scenario.json");

       // Inject tasks into the platform (before finalize!)
       auto tasks = io::inject_scenario(engine, scenario);

       // Schedule all job arrivals
       for (std::size_t i = 0; i < scenario.tasks.size(); ++i) {
           io::schedule_arrivals(engine, *tasks[i], scenario.tasks[i].jobs);
       }

       // Finalize and create scheduler
       engine.platform().finalize();

       std::vector<core::Processor*> procs;
       for (std::size_t i = 0; i < engine.platform().processor_count(); ++i) {
           procs.push_back(&engine.platform().processor(i));
       }
       algo::EdfScheduler scheduler(engine, procs);

       for (auto* task : tasks) {
           scheduler.add_server(*task);
       }

       algo::SingleSchedulerAllocator allocator(engine, scheduler);
       engine.run();

       return 0;
   }

Python Example
^^^^^^^^^^^^^^

.. code-block:: python

   import pyschedsim as sim

   engine = sim.Engine()

   # Load platform and scenario
   sim.load_platform(engine, "platforms/exynos5422LITTLE.json")
   scenario = sim.load_scenario("scenario.json")

   # Inject tasks (before finalize)
   tasks = sim.inject_scenario(engine, scenario)

   # Schedule arrivals
   for i, tp in enumerate(scenario.tasks):
       sim.schedule_arrivals(engine, tasks[i], tp.jobs)

   # Finalize and create scheduler
   engine.platform.finalize()

   procs = sim.get_all_processors(engine)
   scheduler = sim.EdfScheduler(engine, procs)
   for task in tasks:
       scheduler.add_server(task)

   allocator = sim.SingleSchedulerAllocator(engine, scheduler)
   engine.run()

Loading from Strings
^^^^^^^^^^^^^^^^^^^^

Both platforms and scenarios can be loaded from in-memory JSON strings,
useful for testing or dynamically generated configurations:

.. code-block:: python

   platform_json = '{"clusters": [{"name": "cpu", ...}]}'
   sim.load_platform_from_string(engine, platform_json)

   scenario_json = '{"tasks": [...]}'
   scenario = sim.load_scenario_from_string(scenario_json)

Programmatic Scenario Generation (C++)
--------------------------------------

The ``schedsim::io`` library provides functions to generate random task sets
using the UUniFast-Discard algorithm.

.. note::

   Scenario generation is currently C++ only. The ``generate_scenario`` and
   ``uunifast_discard`` functions are not exposed in the Python bindings.

Basic Generation
^^^^^^^^^^^^^^^^

``generate_scenario()`` creates a complete scenario with tasks and job arrivals:

.. code-block:: cpp

   #include <schedsim/io/scenario_generation.hpp>
   #include <schedsim/io/scenario_loader.hpp>
   #include <random>

   using namespace schedsim;

   int main() {
       std::mt19937 rng(42);  // reproducible seed

       // Period distribution: log-uniform between 10ms and 1s
       io::PeriodDistribution period_dist{
           .min = core::duration_from_seconds(0.01),
           .max = core::duration_from_seconds(1.0),
           .log_uniform = true,
       };

       auto scenario = io::generate_scenario(
           10,                                   // number of tasks
           3.5,                                  // target total utilization
           period_dist,                          // period distribution
           core::duration_from_seconds(100.0),   // simulation duration
           rng                                   // RNG
       );

       // Write to file
       io::write_scenario(scenario, "generated.json");

       return 0;
   }

The UUniFast-Discard algorithm distributes the target utilization across tasks
while discarding any set where an individual task utilization exceeds 1.0.

Weibull Job Models
^^^^^^^^^^^^^^^^^^

For more realistic workloads, ``generate_uunifast_discard_weibull()`` produces
jobs whose actual execution times follow a Weibull distribution relative to
the WCET:

.. code-block:: cpp

   io::WeibullJobConfig weibull{
       .success_rate = 1.0,       // fraction of jobs that complete within WCET
       .compression_rate = 1.0,   // shape parameter scaling
   };

   auto scenario = io::generate_uunifast_discard_weibull(
       10,      // num_tasks
       3.5,     // target_utilization
       0.0,     // umin per task
       1.0,     // umax per task
       weibull,
       rng
   );

Using the ``schedgen`` CLI
--------------------------

The ``schedgen`` command-line tool wraps the generation functions:

.. code-block:: bash

   # Generate 10 tasks with total utilization 3.9, max per-task util 1.0
   # Log-uniform periods between 10ms and 1s, simulate for 100s
   schedgen -n 10 -u 3.9 --umax 1 \
            --period-mode range -d 100 \
            -o scenario.json

   # Harmonic periods (powers of 2 from a base)
   schedgen -n 8 -u 2.5 --umax 1 \
            --period-mode harmonic \
            -o scenario.json

Writing Scenarios
-----------------

Programmatically built or modified scenarios can be written back to JSON:

.. code-block:: cpp

   io::write_scenario(scenario, "output.json");

   // Or to stdout
   io::write_scenario_to_stream(scenario, std::cout);

.. code-block:: python

   sim.write_scenario(scenario, "output.json")

Next Steps
----------

- :doc:`first-simulation` --- if you haven't set up a basic simulation yet
- :doc:`dvfs-energy` --- add DVFS and energy management to your simulation
- :doc:`analysis` --- analyze the traces produced by your simulation
