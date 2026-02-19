Multi-Cluster Allocation
========================

Real hardware often has multiple clusters of processors with different
performance characteristics (e.g., ARM big.LITTLE). Schedsim's multi-cluster
allocators assign tasks to clusters based on utilization-aware heuristics.

.. note::

   Multi-cluster allocation is **C++ only**. The ``MultiClusterAllocator``,
   ``FirstFitAllocator``, ``WorstFitAllocator``, and ``BestFitAllocator``
   classes are not exposed in the Python bindings.

Concepts
--------

A **Cluster** wraps a set of processors sharing a clock domain, together with
an EDF scheduler. Clusters track:

- **Performance score** (``perf``): relative speed compared to a reference
  (e.g., 0.5 for LITTLE, 1.0 for big)
- **Scale speed** (``scale_speed``): derived from the clock domain's max
  frequency and the reference frequency
- **Utilization target** (``u_target``): optional cap on how full the cluster
  can be (defaults to the number of processors)

A **MultiClusterAllocator** receives job arrivals and delegates each task to
a cluster selected by the ``select_cluster()`` strategy. The built-in
strategies are:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Allocator
     - Strategy
   * - ``FirstFitAllocator``
     - Assigns to the first cluster with enough remaining capacity
   * - ``WorstFitAllocator``
     - Assigns to the cluster with the most remaining capacity
   * - ``BestFitAllocator``
     - Assigns to the cluster with the least remaining capacity that still fits

Building a Heterogeneous Platform
---------------------------------

The setup for multi-cluster allocation:

1. Load or build a heterogeneous platform (multiple clock domains)
2. Create one ``EdfScheduler`` per cluster
3. Wrap each scheduler + clock domain in a ``Cluster``
4. Pass all clusters to a ``FirstFitAllocator`` (or another strategy)

.. code-block:: cpp

   #include <schedsim/core/engine.hpp>
   #include <schedsim/core/platform.hpp>
   #include <schedsim/core/types.hpp>
   #include <schedsim/algo/edf_scheduler.hpp>
   #include <schedsim/algo/cluster.hpp>
   #include <schedsim/algo/first_fit_allocator.hpp>
   #include <schedsim/algo/worst_fit_allocator.hpp>
   #include <schedsim/algo/best_fit_allocator.hpp>
   #include <schedsim/io/scenario_loader.hpp>
   #include <schedsim/io/scenario_injection.hpp>
   #include <schedsim/io/trace_writers.hpp>

   #include <memory>
   #include <vector>

   using namespace schedsim;

   int main() {
       core::Engine engine;
       auto& platform = engine.platform();

       // --- LITTLE cluster: 4 cores, 500--1400 MHz, perf=0.5 ---
       auto& little_type = platform.add_processor_type("A7", 0.5);
       auto& little_clock = platform.add_clock_domain(
           core::Frequency{500.0}, core::Frequency{1400.0},
           core::duration_from_seconds(0.0001));
       auto& little_power = platform.add_power_domain({
           {0, core::CStateScope::PerProcessor,
            core::duration_from_seconds(0.0), core::Power{200.0}},
           {1, core::CStateScope::PerProcessor,
            core::duration_from_seconds(0.001), core::Power{5.0}},
       });
       for (int i = 0; i < 4; ++i) {
           platform.add_processor(little_type, little_clock, little_power);
       }

       // --- big cluster: 4 cores, 800--2000 MHz, perf=1.0 ---
       auto& big_type = platform.add_processor_type("A15", 1.0);
       auto& big_clock = platform.add_clock_domain(
           core::Frequency{800.0}, core::Frequency{2000.0},
           core::duration_from_seconds(0.0001));
       auto& big_power = platform.add_power_domain({
           {0, core::CStateScope::PerProcessor,
            core::duration_from_seconds(0.0), core::Power{800.0}},
           {1, core::CStateScope::PerProcessor,
            core::duration_from_seconds(0.001), core::Power{10.0}},
       });
       for (int i = 0; i < 4; ++i) {
           platform.add_processor(big_type, big_clock, big_power);
       }

       // Load and inject a scenario
       auto scenario = io::load_scenario("scenario.json");
       auto tasks = io::inject_scenario(engine, scenario);
       for (std::size_t i = 0; i < scenario.tasks.size(); ++i) {
           io::schedule_arrivals(engine, *tasks[i], scenario.tasks[i].jobs);
       }

       platform.finalize();

       // The reference frequency is the highest freq_max across all domains
       double ref_freq_max = 2000.0;

       // --- Create per-cluster schedulers ---
       // LITTLE cluster: processors 0--3
       std::vector<core::Processor*> little_procs;
       for (std::size_t i = 0; i < 4; ++i) {
           little_procs.push_back(&platform.processor(i));
       }
       auto little_sched = std::make_unique<algo::EdfScheduler>(engine, little_procs);

       // big cluster: processors 4--7
       std::vector<core::Processor*> big_procs;
       for (std::size_t i = 4; i < 8; ++i) {
           big_procs.push_back(&platform.processor(i));
       }
       auto big_sched = std::make_unique<algo::EdfScheduler>(engine, big_procs);

       // --- Wrap in Cluster objects ---
       auto little_cluster = std::make_unique<algo::Cluster>(
           little_clock, *little_sched, 0.5, ref_freq_max);
       auto big_cluster = std::make_unique<algo::Cluster>(
           big_clock, *big_sched, 1.0, ref_freq_max);

       // Optional: cap LITTLE cluster utilization
       little_cluster->set_u_target(3.2);  // 80% of 4 cores

       // --- Create the allocator ---
       std::vector<algo::Cluster*> clusters = {
           little_cluster.get(), big_cluster.get()
       };
       algo::FirstFitAllocator allocator(engine, clusters);

       // Run
       io::MemoryTraceWriter trace_writer;
       engine.set_trace_writer(&trace_writer);
       engine.run();

       return 0;
   }

Per-Cluster Mode vs Per-Core Mode
----------------------------------

The example above uses **per-cluster mode**: each ``Cluster`` wraps all
processors in a clock domain, and the EDF scheduler manages them collectively.

For finer-grained control, **per-core mode** creates one ``Cluster`` per
processor. Each cluster has a single-processor scheduler:

.. code-block:: cpp

   std::vector<std::unique_ptr<algo::EdfScheduler>> schedulers;
   std::vector<std::unique_ptr<algo::Cluster>> clusters;
   std::vector<algo::Cluster*> cluster_ptrs;

   for (std::size_t i = 0; i < platform.processor_count(); ++i) {
       auto& proc = platform.processor(i);
       auto sched = std::make_unique<algo::EdfScheduler>(
           engine, std::vector<core::Processor*>{&proc});

       double perf = proc.type().performance();
       auto cluster = std::make_unique<algo::Cluster>(
           proc.clock_domain(), *sched, perf, ref_freq_max);
       cluster->set_processor_id(proc.id());  // mark as per-core

       cluster_ptrs.push_back(cluster.get());
       schedulers.push_back(std::move(sched));
       clusters.push_back(std::move(cluster));
   }

   algo::FirstFitAllocator allocator(engine, cluster_ptrs);

Choosing an Allocator
---------------------

Switching the allocation strategy requires only changing the allocator type:

.. code-block:: cpp

   // First-fit: assigns to the first cluster that has room
   algo::FirstFitAllocator ff_alloc(engine, clusters);

   // Worst-fit: spreads tasks across clusters (most remaining capacity)
   algo::WorstFitAllocator wf_alloc(engine, clusters);

   // Best-fit: packs tasks tightly (least remaining capacity that fits)
   algo::BestFitAllocator bf_alloc(engine, clusters);

All allocators share the same constructor signature and can be swapped
in place.

Enabling DVFS per Cluster
-------------------------

DVFS and reclamation policies are configured per scheduler. In a multi-cluster
setup, enable them on each scheduler individually:

.. code-block:: cpp

   little_sched->enable_grub();
   little_sched->enable_ffa(core::duration_from_seconds(0.001), 1);

   big_sched->enable_grub();
   big_sched->enable_ffa(core::duration_from_seconds(0.001), 1);

See :doc:`dvfs-energy` for details on the available policies.

Cluster Queries
---------------

Clusters provide utilization queries that account for heterogeneous performance
scaling:

.. code-block:: cpp

   // Total utilization of tasks assigned to this cluster
   double u = cluster->utilization();

   // Remaining capacity (u_target - utilization)
   double remaining = cluster->remaining_capacity();

   // Check if a task with given budget/period can be admitted
   bool ok = cluster->can_admit(
       core::duration_from_seconds(0.003),
       core::duration_from_seconds(0.010));

   // Utilization scaled by the cluster's performance relative to reference
   double scaled = cluster->scaled_utilization(0.3);

Next Steps
----------

- :doc:`dvfs-energy` --- DVFS and DPM policy details
- :doc:`analysis` --- measure cluster migration counts and per-processor metrics
