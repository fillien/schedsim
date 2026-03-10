I/O and Scenario Management
===========================

The ``pyschedsim`` module exposes functions and data classes for loading
platform definitions, loading and generating workload scenarios, injecting
tasks into the engine, and writing scenarios back to disk.

For the full C++ I/O reference, see :doc:`../api-cpp/io`.

Platform Loading
----------------

.. py:function:: load_platform(engine: Engine, path: str) -> None

   Load a platform definition from a JSON file and configure the engine.

   The platform must not have been finalized before calling this function.
   Both the new component-based format and the legacy cluster-based format
   are supported; the loader detects the format automatically. See
   :doc:`../json-formats/index` for a description of both formats.

   :param engine: The simulation engine to configure.
   :param path: Path to a platform JSON file.
   :raises LoaderError: If the file cannot be opened or the JSON is invalid.

.. py:function:: load_platform_from_string(engine: Engine, json: str) -> None

   Load a platform definition from a JSON string.

   Identical to :py:func:`load_platform` but reads from an in-memory string
   instead of a file. Useful for testing or dynamically constructed
   configurations.

   :param engine: The simulation engine to configure.
   :param json: A JSON string in platform format.
   :raises LoaderError: If the JSON is invalid or missing required fields.

Scenario Loading
----------------

Data Classes
^^^^^^^^^^^^

.. py:class:: ScenarioData

   Container for a complete task set with job arrivals.

   .. py:attribute:: tasks
      :type: list[TaskParams]

      The list of tasks in this scenario. Each element describes one
      periodic real-time task and its concrete job instances.

.. py:class:: TaskParams

   Parameters for one periodic real-time task.

   .. py:attribute:: id
      :type: int

      Task identifier. 1-based by convention; preserved through load and
      inject operations.

   .. py:attribute:: period
      :type: float

      Inter-arrival period in seconds. Must be positive.

   .. py:attribute:: relative_deadline
      :type: float

      Relative deadline in seconds. Defaults to ``period`` (implicit
      deadline) when loading from JSON if the field is absent.

   .. py:attribute:: wcet
      :type: float

      Worst-case execution time in seconds. Must be positive.

   .. py:attribute:: jobs
      :type: list[JobParams]

      Concrete job instances for this task, sorted by arrival time.

.. py:class:: JobParams

   Parameters for a single job instance.

   .. py:attribute:: arrival
      :type: float

      Absolute arrival time in seconds.

   .. py:attribute:: duration
      :type: float

      Actual execution demand for this job in seconds. Must be positive.
      May exceed ``wcet`` --- the loader does not enforce this constraint.

Loading Functions
^^^^^^^^^^^^^^^^^

.. py:function:: load_scenario(path: str) -> ScenarioData

   Load a scenario (task set with job arrivals) from a JSON file.

   The returned :py:class:`ScenarioData` object owns its data and can be
   modified before injection. See :doc:`../json-formats/index` for the
   scenario file format.

   :param path: Path to a scenario JSON file.
   :returns: A :py:class:`ScenarioData` populated from the file.
   :raises LoaderError: If the file cannot be opened or the JSON is invalid.

.. py:function:: load_scenario_from_string(json: str) -> ScenarioData

   Load a scenario from a JSON string.

   Identical to :py:func:`load_scenario` but reads from an in-memory string.

   :param json: A JSON string in scenario format.
   :returns: A :py:class:`ScenarioData` populated from the string.
   :raises LoaderError: If the JSON is invalid or missing required fields.

Scenario Injection and Scheduling
----------------------------------

.. admonition:: Ordering constraints

   - :py:func:`inject_scenario` must be called **before**
     ``platform.finalize()`` because it adds tasks to the platform.
   - :py:func:`schedule_arrivals` can be called either before or after
     ``platform.finalize()``.
   - The scheduler and allocator must be created **after** finalize.

.. py:function:: inject_scenario(engine: Engine, scenario: ScenarioData) -> list[Task]

   Create :py:class:`Task` objects in the platform from scenario data.

   For each :py:class:`TaskParams` in *scenario*, a task is added to the
   platform with the corresponding ``period``, ``relative_deadline``, and
   ``wcet``. The task IDs from the scenario are preserved.

   **Must be called before** ``platform.finalize()``.

   :param engine: The simulation engine.
   :param scenario: A loaded or generated :py:class:`ScenarioData`.
   :returns: A list of :py:class:`Task` references, one per task in
             *scenario*, in the same order as ``scenario.tasks``.

.. py:function:: schedule_arrivals(engine: Engine, task: Task, jobs: list[JobParams]) -> None

   Schedule job arrivals for a task.

   For each :py:class:`JobParams` in *jobs*, a job arrival event is
   registered in the engine's event queue at the specified time with the
   given execution demand.

   Can be called before or after ``platform.finalize()``.

   :param engine: The simulation engine.
   :param task: The task to schedule arrivals for.
   :param jobs: The list of job parameters to schedule.

Writing Scenarios
-----------------

.. py:function:: write_scenario(scenario: ScenarioData, path: str) -> None

   Write a scenario to a JSON file.

   The output always uses the canonical ``wcet`` field (never
   ``utilization``). Jobs are written in ascending arrival-time order.

   :param scenario: The :py:class:`ScenarioData` to serialize.
   :param path: Output file path. The file is created or overwritten.

Scenario Generation
-------------------

.. py:class:: WeibullJobConfig

   Parameters controlling the Weibull distribution used to sample job
   execution times relative to the task WCET.

   .. py:attribute:: success_rate
      :type: float

      Fraction of jobs that complete within their WCET. A value of 1.0
      means all jobs finish at or before their WCET; values below 1.0
      introduce overruns.

   .. py:attribute:: compression_rate
      :type: float

      Shape parameter scaling for the Weibull distribution. Higher values
      produce execution times clustered more tightly around the mean.

.. py:function:: from_utilizations(utilizations: list[float], success_rate: float = 1.0, compression_rate: float = 1.0, seed: int = 42) -> ScenarioData

   Generate a random task set from a list of per-task utilization values.

   One task is created per entry in *utilizations*. Periods are drawn
   log-uniformly, WCETs are computed as ``period * utilization``, and
   job execution times are sampled from a Weibull distribution controlled
   by *success_rate* and *compression_rate*.

   :param utilizations: List of per-task utilization values, each in (0, 1].
   :param success_rate: Fraction of jobs completing within WCET (default 1.0).
   :param compression_rate: Weibull shape scaling (default 1.0).
   :param seed: RNG seed for reproducibility (default 42).
   :returns: A :py:class:`ScenarioData` with one task per utilization entry.

.. note::

   The C++ API provides additional scenario generation functions
   (``generate_scenario``, ``uunifast_discard``) that are not exposed in
   the Python bindings. However, :py:func:`from_utilizations` covers most
   common use cases. See :doc:`../api-cpp/io` for the full C++ I/O
   reference.

Examples
--------

Full Workflow: Load, Inject, and Schedule
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The typical three-step process for running a scenario from a JSON file:

.. code-block:: python

   import pyschedsim as sim

   engine = sim.Engine()

   # Step 1: Load the hardware platform
   sim.load_platform(engine, "platforms/exynos5422LITTLE.json")

   # Step 2: Load the scenario (task set with job arrivals)
   scenario = sim.load_scenario("scenario.json")

   # Step 3: Inject tasks into the platform -- must be BEFORE finalize
   tasks = sim.inject_scenario(engine, scenario)

   # Step 4: Schedule job arrivals for every task
   for i, tp in enumerate(scenario.tasks):
       sim.schedule_arrivals(engine, tasks[i], tp.jobs)

   # Step 5: Finalize the platform and build the scheduler
   engine.platform.finalize()

   procs = sim.get_all_processors(engine)
   scheduler = sim.EdfScheduler(engine, procs)
   for task in tasks:
       scheduler.add_server(task)

   allocator = sim.SingleSchedulerAllocator(engine, scheduler)

   writer = sim.MemoryTraceWriter()
   engine.set_trace_writer(writer)
   engine.run()

   metrics = writer.compute_metrics()
   print(f"Completed: {metrics.completed_jobs}/{metrics.total_jobs}")
   print(f"Deadline misses: {metrics.deadline_misses}")

Programmatic Generation with ``from_utilizations``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When you do not have a pre-recorded scenario file, generate one directly
from a list of per-task utilizations:

.. code-block:: python

   import pyschedsim as sim

   # Three tasks with utilizations 0.3, 0.25, and 0.2
   scenario = sim.from_utilizations(
       [0.3, 0.25, 0.2],
       success_rate=0.95,      # 5 % of jobs may overrun
       compression_rate=1.0,
       seed=42,
   )

   print(f"Generated {len(scenario.tasks)} tasks")
   for tp in scenario.tasks:
       print(f"  Task {tp.id}: period={tp.period:.4f}s  "
             f"wcet={tp.wcet:.4f}s  jobs={len(tp.jobs)}")

   # Inject and simulate as usual
   engine = sim.Engine()
   sim.load_platform(engine, "platforms/exynos5422LITTLE.json")
   tasks = sim.inject_scenario(engine, scenario)
   for i, tp in enumerate(scenario.tasks):
       sim.schedule_arrivals(engine, tasks[i], tp.jobs)

   engine.platform.finalize()

Writing a Modified Scenario
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Scenarios can be serialised back to JSON after loading or programmatic
construction:

.. code-block:: python

   import pyschedsim as sim

   # Load, optionally modify, then save
   scenario = sim.load_scenario("original.json")

   # Example: trim jobs beyond 1 second
   for tp in scenario.tasks:
       tp.jobs = [j for j in tp.jobs if j.arrival < 1.0]

   sim.write_scenario(scenario, "trimmed.json")

See Also
--------

- :doc:`../json-formats/index` --- platform and scenario JSON format reference
- :doc:`../api-cpp/io` --- full C++ I/O API
- :doc:`../tutorials/scenarios` --- step-by-step scenario tutorial
