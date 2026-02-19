CLI Reference
=============

Schedsim provides four command-line tools for generating task sets, running
simulations, analyzing traces, and testing multi-cluster allocators.

.. contents:: Tools
   :local:
   :depth: 2

``schedsim``
------------

Run a real-time scheduling simulation on a given platform and scenario.

.. code-block:: bash

   schedsim -i scenario.json -p platform.json [options]

Options
^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Option
     - Default
     - Description
   * - ``-i, --input``
     - (required)
     - Scenario file (JSON)
   * - ``-p, --platform``
     - (required)
     - Platform configuration file (JSON)
   * - ``-s, --scheduler``
     - ``edf``
     - Scheduling algorithm. Currently only ``edf`` is available.
   * - ``--reclaim``
     - ``none``
     - Bandwidth reclamation: ``none``, ``grub``, or ``cash``
   * - ``--dvfs``
     - ``none``
     - DVFS policy: ``none``, ``power-aware``, ``ffa``, or ``csf``
   * - ``--dvfs-cooldown``
     - ``0``
     - Minimum time between DVFS transitions (milliseconds)
   * - ``--dpm``
     - ``none``
     - DPM policy: ``none`` or ``basic``
   * - ``--dpm-cstate``
     - ``1``
     - Target C-state level for DPM idle transitions
   * - ``-d, --duration``
     - auto
     - Simulation duration in seconds. When ``0`` (default), the simulation
       runs until no events remain.
   * - ``--energy``
     -
     - Enable energy tracking
   * - ``--context-switch``
     -
     - Enable context switch overhead modelling
   * - ``-o, --output``
     - ``-`` (stdout)
     - Trace output file path
   * - ``--format``
     - ``json``
     - Output format: ``json`` or ``null`` (discard trace)
   * - ``--metrics``
     -
     - Print summary metrics to stderr after simulation
   * - ``-v, --verbose``
     -
     - Verbose progress messages on stderr
   * - ``-h, --help``
     -
     - Show help and exit

Behavioural Notes
^^^^^^^^^^^^^^^^^

- When ``--dvfs`` is ``ffa`` or ``csf``, the ``--dpm basic`` option is
  silently ignored because FFA and CSF manage DPM internally.
- The ``--dpm-cstate`` value is passed to FFA/CSF even though ``--dpm`` is
  ignored, controlling the idle C-state used by those policies.

Exit Codes
^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Code
     - Meaning
   * - ``0``
     - Success
   * - ``1``
     - Configuration or runtime error (bad JSON, I/O failure)
   * - ``2``
     - Admission control failure (task set exceeds schedulable bound)
   * - ``64``
     - Invalid command-line arguments

Example
^^^^^^^

.. code-block:: bash

   # EDF with GRUB reclamation and FFA DVFS on Exynos 5422
   schedsim -i scenario.json -p platforms/exynos5422.json \
       --reclaim grub --dvfs ffa --dvfs-cooldown 1 \
       --energy -o trace.json

See :doc:`/tutorials/first-simulation` and :doc:`/tutorials/dvfs-energy` for
step-by-step walkthroughs.


``schedgen``
------------

Generate random task sets using UUniFast-Discard with configurable period
distributions and optional Weibull-distributed job execution times.

.. code-block:: bash

   schedgen -n 10 -u 3.0 [options]

Options
^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Option
     - Default
     - Description
   * - ``-n, --tasks`` (or ``-t``)
     - (required)
     - Number of tasks to generate
   * - ``-u, --utilization`` (or ``--totalu``)
     - (required)
     - Target total utilization (can exceed 1.0 for multicore)
   * - ``--umin``
     - ``0``
     - Minimum per-task utilization [0, 1]
   * - ``--umax``
     - ``1``
     - Maximum per-task utilization [0, 1]
   * - ``-s, --success``
     - ``1``
     - Weibull success rate for deadline budget [0, 1]
   * - ``-c, --compression``
     - ``1``
     - Compression ratio (min job duration / WCET) [0, 1]
   * - ``--period-mode``
     - ``range``
     - Period selection: ``range`` (random from interval) or ``harmonic``
       (fixed harmonic set)
   * - ``--period-min``
     - ``10``
     - Minimum period in ms (range mode only)
   * - ``--period-max``
     - ``1000``
     - Maximum period in ms (range mode only)
   * - ``--log-uniform``
     -
     - Use log-uniform period distribution (default for range mode)
   * - ``--uniform``
     -
     - Use uniform period distribution (range mode only)
   * - ``-d, --duration``
     - ``0``
     - Simulation duration in seconds for job generation (range mode only).
       When ``0``, tasks are generated without jobs.
   * - ``--exec-ratio``
     - ``1.0``
     - Actual execution / WCET ratio for job durations (range mode only)
   * - ``-o, --output``
     - ``-`` (stdout)
     - Output file path
   * - ``--seed``
     -
     - Random seed for reproducibility
   * - ``-h, --help``
     -
     - Show help and exit

Generation Modes
^^^^^^^^^^^^^^^^

**Range mode** (default) draws periods uniformly or log-uniformly from
``[period-min, period-max]``. WCET is derived from the UUniFast utilization
split. If ``--duration`` is specified, concrete jobs are generated up to that
time with execution times scaled by ``--exec-ratio``.

**Harmonic mode** uses a fixed set of harmonic periods and Weibull-distributed
job execution times controlled by ``--success`` and ``--compression``.

Batch Mode
^^^^^^^^^^

Generate multiple task sets in parallel:

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Option
     - Default
     - Description
   * - ``--batch`` (or ``-T, --tasksets``)
     -
     - Number of scenarios to generate
   * - ``--dir``
     - (required with ``--batch``)
     - Output directory (created if it does not exist)
   * - ``--threads``
     - hardware concurrency
     - Number of parallel threads

Files are named ``1.json``, ``2.json``, ... in the output directory.

Validation Rules
^^^^^^^^^^^^^^^^

- ``num_tasks >= 1``
- ``utilization > 0``
- ``umin`` and ``umax`` must be in [0, 1] and ``umin <= umax``
- ``num_tasks * umin <= utilization <= num_tasks * umax`` (feasibility)
- In range mode: ``period-min <= period-max``, ``exec-ratio`` in (0, 1],
  ``utilization <= num_tasks``

Examples
^^^^^^^^

.. code-block:: bash

   # 10 implicit-deadline tasks, total U=3.0, log-uniform periods, with jobs
   schedgen -n 10 -u 3.0 --period-min 5 --period-max 200 -d 1.0 -o taskset.json

   # Harmonic task set (Weibull jobs, 90% success rate)
   schedgen -n 8 -u 2.5 --period-mode harmonic -s 0.9 -c 0.5 -o taskset.json

   # Batch: 1000 task sets with 4 threads
   schedgen -n 10 -u 3.5 -d 0.5 --batch 1000 --dir scenarios/ --threads 4

See :doc:`/tutorials/scenarios` for detailed usage.


``schedview``
-------------

Compute scheduling metrics from simulation trace files.

.. code-block:: bash

   schedview trace.json [options]

Options
^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Option
     - Default
     - Description
   * - ``<trace-file>``
     -
     - Positional argument: path to a JSON trace file
   * - ``-d, --directory``
     -
     - Process all ``*.json`` trace files in a directory (batch mode)
   * - ``--deadline-misses``
     -
     - Show per-task deadline miss details
   * - ``--response-times``
     -
     - Show per-task response time statistics
   * - ``--energy``
     -
     - Show energy breakdown per processor
   * - ``--format``
     - ``text``
     - Output format: ``text``, ``csv``, or ``json``
   * - ``-h, --help``
     -
     - Show help and exit

Either a positional trace file or ``--directory`` is required.

Batch Mode
^^^^^^^^^^

When ``--directory`` is given, ``schedview`` processes all ``*.json`` files in
the directory (sorted alphabetically) and produces combined output. In CSV
format, each row contains the filename plus all metrics including extra columns
not shown in single-file mode:

- ``transitions`` --- frequency transition events
- ``cluster_migrations`` --- tasks moved between clusters
- ``core_state_requests`` --- C-state change requests
- ``frequency_requests`` --- DVFS frequency change requests

Examples
^^^^^^^^

.. code-block:: bash

   # Text summary with energy
   schedview trace.json --energy

   # CSV output with full stats
   schedview trace.json --format csv --energy --response-times

   # Batch: all traces in a directory
   schedview -d traces/ --format csv --energy > results.csv

See :doc:`/tutorials/analysis` for metrics interpretation.


``alloc``
---------

Test multi-cluster task allocation strategies. Runs a simulation with the
chosen allocator and reports the allocation result.

.. code-block:: bash

   alloc -i scenario.json -p platform.json -a ff_big_first [options]

Options
^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Option
     - Default
     - Description
   * - ``-i, --input``
     - (required)
     - Scenario file (JSON)
   * - ``-p, --platform``
     - (required)
     - Platform configuration file (JSON)
   * - ``-a, --alloc``
     - (required)
     - Allocator name (see table below)
   * - ``-g, --granularity``
     - ``per-cluster``
     - Allocation granularity: ``per-cluster`` or ``per-core``
   * - ``-A, --alloc-arg``
     -
     - Allocator argument as ``key=value`` (repeatable)
   * - ``--target``
     -
     - Utilization target for LITTLE clusters (perf < 1.0)
   * - ``--reclaim``
     - ``none``
     - Reclamation: ``none``, ``grub``, or ``cash``
   * - ``--dvfs``
     - ``none``
     - DVFS policy: ``none``, ``power-aware``, ``ffa``, ``csf``,
       ``ffa-timer``, or ``csf-timer``
   * - ``--dvfs-cooldown``
     - ``0``
     - DVFS cooldown in ms
   * - ``-v, --verbose``
     -
     - Verbose progress on stderr
   * - ``-h, --help``
     -
     - Show help and exit

Allocators
^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Name
     - Description
   * - ``ff_big_first``
     - First-fit, big cluster first
   * - ``ff_little_first``
     - First-fit, little cluster first
   * - ``ff_cap``
     - First-fit with capacity checking
   * - ``ff_cap_adaptive_linear``
     - Capacity-aware with linear adaptation
   * - ``ff_cap_adaptive_poly``
     - Capacity-aware with polynomial adaptation
   * - ``ff_lb``
     - First-fit with load balancing
   * - ``counting``
     - Counts allocations without simulating (result = allocation count)
   * - ``first_fit``
     - Classic first-fit
   * - ``worst_fit``
     - Worst-fit (most remaining capacity)
   * - ``best_fit``
     - Best-fit (least remaining capacity)
   * - ``mcts``
     - Monte Carlo tree search with a fixed pattern

Allocator Arguments
^^^^^^^^^^^^^^^^^^^

The ``--alloc-arg`` option passes key-value pairs to allocators. Currently
only ``mcts`` uses this:

.. code-block:: bash

   alloc -i s.json -p p.json -a mcts -A pattern=0,1,0,1

The ``pattern`` value is a comma-separated list of cluster indices.

Behavioural Notes
^^^^^^^^^^^^^^^^^

- In ``per-core`` mode, ``--reclaim`` and ``--dvfs`` are ignored with a
  warning on stderr.
- ``--dvfs`` supports ``ffa-timer`` and ``csf-timer`` modes that are not
  available in ``schedsim``.
- The ``counting`` allocator returns the number of allocations instead of
  rejected tasks.
- The ``ff_cap_adaptive_linear`` and ``ff_cap_adaptive_poly`` allocators
  automatically receive the total scenario utilization for their adaptation
  logic.

Output Format
^^^^^^^^^^^^^

A single semicolon-separated line per run:

.. code-block:: text

   <scenario-file>;<allocator>;<result>

Where ``<result>`` is the number of rejected tasks (or allocation count for
``counting``). For ``ff_cap`` with ``--target``, the allocator label includes
the target value (e.g. ``ff_cap_0.5``).

Exit Codes
^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Code
     - Meaning
   * - ``0``
     - Success
   * - ``1``
     - Configuration or runtime error
   * - ``2``
     - Admission control failure
   * - ``64``
     - Invalid command-line arguments

Example
^^^^^^^

.. code-block:: bash

   # Test first-fit big-first with GRUB on Exynos 5422
   alloc -i scenario.json -p platforms/exynos5422.json \
       -a ff_big_first --reclaim grub

   # Per-core granularity
   alloc -i scenario.json -p platforms/orion6.json \
       -a worst_fit -g per-core
