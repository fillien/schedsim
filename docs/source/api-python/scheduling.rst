Scheduling API
==============

The scheduling module covers the EDF scheduler, CBS bandwidth servers,
multi-cluster support, and task allocators.

For the equivalent C++ types, see :doc:`../api-cpp/algo`.

----

Enumerations
------------

AdmissionTest
^^^^^^^^^^^^^

.. py:class:: AdmissionTest

   Selects which utilization bound the scheduler uses when deciding whether a
   new CBS server can be admitted.

   .. py:attribute:: CapacityBound

      Admit a server if the total utilization does not exceed the number of
      processors (``U_total ≤ m``).

   .. py:attribute:: GFB

      Admit a server using the Global Feasibility Bound test, which gives a
      tighter bound for EDF on multiprocessors.

DeadlineMissPolicy
^^^^^^^^^^^^^^^^^^

.. py:class:: DeadlineMissPolicy

   Controls what the scheduler does when a job misses its absolute deadline.

   .. py:attribute:: Continue

      Ignore the miss and continue executing the job normally.

   .. py:attribute:: AbortJob

      Discard the current job instance but keep the task active.

   .. py:attribute:: AbortTask

      Remove the task and all its pending jobs from the scheduler.

   .. py:attribute:: StopSimulation

      Halt the simulation immediately.

----

EdfScheduler
------------

.. py:class:: EdfScheduler(engine: Engine, processors: list[Processor])

   An Earliest Deadline First scheduler managing one or more processors. CBS
   servers are added after construction; policies are layered on top in order.

   :param engine: The simulation engine.
   :param processors: The processors this scheduler manages.

   .. admonition:: Policy ordering

      1. Create the scheduler and add CBS servers with ``add_server()``.
      2. Enable a reclamation policy: ``enable_grub()`` or ``enable_cash()``.
      3. Enable DVFS: ``enable_power_aware_dvfs()`` or a combined policy
         (FFA/CSF).
      4. Enable DPM: ``enable_basic_dpm()`` (only when *not* using a combined
         policy).
      5. Create the allocator.

   .. rubric:: Properties

   .. py:attribute:: utilization
      :type: float

      Total CBS utilization across all admitted servers (read-only).

   .. py:attribute:: processor_count
      :type: int

      Number of processors managed by this scheduler (read-only).

   .. py:attribute:: server_count
      :type: int

      Number of CBS servers currently registered (read-only).

   .. py:attribute:: active_utilization
      :type: float

      Utilization of currently active (non-idle) servers (read-only).

   .. py:attribute:: scheduler_utilization
      :type: float

      Current aggregate utilization seen by the scheduler (read-only).

   .. py:attribute:: max_scheduler_utilization
      :type: float

      Maximum scheduler utilization recorded since the simulation started
      (read-only).

   .. py:attribute:: max_server_utilization
      :type: float

      Maximum utilization of any single CBS server recorded since the
      simulation started (read-only).

   .. rubric:: Server Management

   .. py:method:: add_server(task: Task) -> CbsServer

      Add a CBS server for *task*, using the task's own WCET as the budget and
      the task's period as the server period.
      Raises :ref:`AdmissionError` if the admission test fails.

   .. py:method:: add_server(task: Task, budget: float, period: float) -> CbsServer

      Add a CBS server for *task* with explicit *budget* and *period* (both in
      **seconds**).
      Raises :ref:`AdmissionError` if the admission test fails.

   .. py:method:: can_admit(budget: float, period: float) -> bool

      Return ``True`` if a server with the given *budget* / *period* (in
      **seconds**) can be admitted without violating the utilization bound.

   .. py:method:: set_admission_test(test: AdmissionTest) -> None

      Select the admission test used by ``add_server()`` and ``can_admit()``.

   .. py:method:: set_deadline_miss_policy(policy: DeadlineMissPolicy) -> None

      Set the action taken when a job misses its absolute deadline.

   .. py:method:: set_expected_arrivals(task: Task, count: int) -> None

      Hint the expected number of job arrivals for *task*. Used by the M-GRUB
      reclamation policy to compute per-task reclaim budgets.

   .. rubric:: Reclamation Policies

   .. py:method:: enable_grub() -> None

      Enable GRUB (Global Reclamation of Unused Bandwidth) reclamation.
      Must be enabled before any DVFS or DPM policy.

   .. py:method:: enable_cash() -> None

      Enable CASH (Capacity Sharing) reclamation. An alternative to GRUB.
      Must be enabled before any DVFS or DPM policy.

   .. rubric:: DVFS Policies

   .. py:method:: enable_power_aware_dvfs(cooldown: float) -> None

      Enable Power-Aware DVFS. Sets the clock domain frequency to
      ``f_max × ((m-1)×u_max + U_total) / m`` on every scheduling event.

      :param cooldown: Minimum interval between frequency changes in
         **seconds**.

   .. rubric:: DPM Policies

   .. py:method:: enable_basic_dpm(target_cstate: int = 1) -> None

      Put idle processors into C-state *target_cstate* when they have no work.

      :param target_cstate: C-state level to use for idle processors
         (default ``1``).

   .. rubric:: Combined DVFS + DPM Policies

   .. note::

      Combined policies (FFA, CSF, and their timer variants) incorporate both
      DVFS and DPM. Do **not** additionally call ``enable_power_aware_dvfs()``
      or ``enable_basic_dpm()`` when using them.

   .. py:method:: enable_ffa(cooldown: float, sleep_cstate: int = 1) -> None

      Enable FFA (Frequency-First Approach): reduces frequency before shutting
      down cores.

      :param cooldown: Minimum interval between policy updates in **seconds**.
      :param sleep_cstate: C-state level for idle processors (default ``1``).

   .. py:method:: enable_csf(cooldown: float, sleep_cstate: int = 1) -> None

      Enable CSF (Core-Shutdown First): shuts down cores before reducing
      frequency.

      :param cooldown: Minimum interval between policy updates in **seconds**.
      :param sleep_cstate: C-state level for idle processors (default ``1``).

   .. py:method:: enable_ffa_timer(cooldown: float, sleep_cstate: int = 1) -> None

      Timer-based variant of FFA: re-evaluates the policy periodically rather
      than on every scheduling event.

      :param cooldown: Timer interval in **seconds**.
      :param sleep_cstate: C-state level for idle processors (default ``1``).

   .. py:method:: enable_csf_timer(cooldown: float, sleep_cstate: int = 1) -> None

      Timer-based variant of CSF.

      :param cooldown: Timer interval in **seconds**.
      :param sleep_cstate: C-state level for idle processors (default ``1``).

Example — EdfScheduler with GRUB reclamation
"""""""""""""""""""""""""""""""""""""""""""""

.. code-block:: python

   import pyschedsim as sim

   engine = sim.Engine()
   platform = engine.platform

   cpu   = platform.add_processor_type("cpu", 1.0)
   clock = platform.add_clock_domain(500.0, 2000.0, 0.0001)  # 100 µs DVFS delay
   power = platform.add_power_domain([
       (0, 0, 0.0,   100.0),
       (1, 0, 0.001,  10.0),
   ])
   platform.add_processor(cpu, clock, power)
   platform.add_processor(cpu, clock, power)

   task1 = platform.add_task(0.010, 0.010, 0.003)
   task2 = platform.add_task(0.020, 0.020, 0.005)

   engine.schedule_job_arrival(task1, 0.0, 0.002)
   engine.schedule_job_arrival(task2, 0.0, 0.004)
   engine.enable_energy_tracking(True)
   platform.finalize()

   procs = sim.get_all_processors(engine)
   scheduler = sim.EdfScheduler(engine, procs)
   scheduler.add_server(task1)
   scheduler.add_server(task2)

   # Enable reclamation, then DVFS
   scheduler.enable_grub()
   scheduler.enable_power_aware_dvfs(0.001)  # 1 ms cooldown

   allocator = sim.SingleSchedulerAllocator(engine, scheduler)
   engine.run()

   print(f"Total energy: {engine.total_energy():.3f} mJ")

----

CbsServer
---------

.. py:class:: CbsServer

   ``CbsServer`` is an opaque handle returned by ``EdfScheduler.add_server()``.
   It represents a Constant Bandwidth Server and is **not directly queryable**
   from Python. Store the reference if you need to identify the server later,
   but no properties or methods are exposed on this object.

----

SingleSchedulerAllocator
------------------------

.. py:class:: SingleSchedulerAllocator(engine: Engine, scheduler: EdfScheduler, clock_domain: ClockDomain | None = None)

   Routes all incoming job-arrival events to a single ``EdfScheduler``.
   Use this allocator for single-cluster (homogeneous) platforms.

   :param engine: The simulation engine.
   :param scheduler: The scheduler that will receive all job arrivals.
   :param clock_domain: Optional clock domain hint (default ``None``).

   .. note::

      The allocator installs the job-arrival handler at construction time.
      It must be created *after* all CBS servers have been added to the
      scheduler. Raises :ref:`HandlerAlreadySetError` if a handler is already
      installed.

----

Cluster
-------

.. py:class:: Cluster(clock_domain: ClockDomain, scheduler: EdfScheduler, perf_score: float, reference_freq_max: float)

   Wraps an ``EdfScheduler`` and a ``ClockDomain`` into a logical cluster for
   use with multi-cluster allocators.  A cluster tracks its own utilization and
   exposes admission queries that account for heterogeneous performance scaling.

   :param clock_domain: The clock domain shared by all processors in this
      cluster.
   :param scheduler: The ``EdfScheduler`` managing the cluster's processors.
   :param perf_score: Relative performance of this cluster (e.g., ``0.5`` for
      LITTLE, ``1.0`` for big).
   :param reference_freq_max: The highest ``freq_max`` across all clusters,
      used to normalise the scale-speed computation.

   .. rubric:: Properties

   .. py:attribute:: perf
      :type: float

      Performance score of this cluster (read-only).

   .. py:attribute:: scale_speed
      :type: float

      Effective speed factor relative to the reference cluster, derived from
      ``perf_score`` and ``clock_domain.freq_max / reference_freq_max``
      (read-only).

   .. py:attribute:: u_target
      :type: float

      Utilization cap for this cluster (read-only). Defaults to the number of
      processors. Use ``set_u_target()`` to override.

   .. py:attribute:: processor_count
      :type: int

      Number of processors in this cluster's scheduler (read-only).

   .. py:attribute:: utilization
      :type: float

      Total utilization of all CBS servers assigned to this cluster
      (read-only).

   .. rubric:: Methods

   .. py:method:: clock_domain() -> ClockDomain

      Return the clock domain associated with this cluster.

   .. py:method:: scheduler() -> EdfScheduler

      Return the scheduler associated with this cluster.

   .. py:method:: set_u_target(target: float) -> None

      Set an explicit utilization cap for this cluster. Tasks will not be
      assigned here once ``utilization`` exceeds *target*.

   .. py:method:: scaled_utilization(task_util: float) -> float

      Return *task_util* scaled by ``scale_speed``, giving the effective
      utilization a task with utilization *task_util* (on the reference
      cluster) would consume on this cluster.

   .. py:method:: can_admit(budget: float, period: float) -> bool

      Return ``True`` if a server with *budget* / *period* (in **seconds**) can
      be admitted to this cluster without exceeding ``u_target``.

   .. note::

      The C++ methods ``remaining_capacity()``, ``set_processor_id()``, and
      ``processor_id()`` are **not** available in the Python bindings.

----

FFCapAllocator
--------------

.. py:class:: FFCapAllocator(engine: Engine, clusters: list[Cluster])

   First-Fit Capacity allocator. Assigns each arriving task to the first
   cluster that has sufficient remaining capacity, querying ``can_admit()``
   on each cluster in order.

   :param engine: The simulation engine.
   :param clusters: Ordered list of clusters to consider.

   .. note::

      The allocator installs the job-arrival handler at construction time.
      It must be created *after* all per-cluster CBS servers have been added.
      Raises :ref:`HandlerAlreadySetError` if a handler is already installed.

----

CountingAllocator
-----------------

.. py:class:: CountingAllocator(engine: Engine, clusters: list[Cluster])

   An allocator that counts how many task assignments have been made. Useful
   for instrumentation and testing.

   :param engine: The simulation engine.
   :param clusters: List of clusters managed by this allocator.

   .. py:method:: allocation_count() -> int

      Return the total number of task allocations performed so far.

.. note::

   The C++ API provides additional allocators (``FirstFitAllocator``,
   ``WorstFitAllocator``, ``BestFitAllocator``, and others) that are not
   currently exposed in the Python bindings. See :doc:`../api-cpp/algo` for
   the full C++ allocator reference.

Example — Multi-cluster setup with FFCapAllocator
""""""""""""""""""""""""""""""""""""""""""""""""""

.. code-block:: python

   import pyschedsim as sim

   engine = sim.Engine()
   platform = engine.platform

   # LITTLE cluster: 4 cores, 500--1400 MHz, perf=0.5
   little_type  = platform.add_processor_type("A7", 0.5)
   little_clock = platform.add_clock_domain(500.0, 1400.0, 0.0001)
   little_power = platform.add_power_domain([
       (0, 0, 0.0,   200.0),
       (1, 0, 0.001,   5.0),
   ])
   for _ in range(4):
       platform.add_processor(little_type, little_clock, little_power)

   # big cluster: 4 cores, 800--2000 MHz, perf=1.0
   big_type  = platform.add_processor_type("A15", 1.0)
   big_clock = platform.add_clock_domain(800.0, 2000.0, 0.0001)
   big_power = platform.add_power_domain([
       (0, 0, 0.0,   800.0),
       (1, 0, 0.001,  10.0),
   ])
   for _ in range(4):
       platform.add_processor(big_type, big_clock, big_power)

   tasks = [platform.add_task(0.010, 0.010, 0.002) for _ in range(6)]
   for t in tasks:
       engine.schedule_job_arrival(t, 0.0, t.wcet)

   platform.finalize()

   # Reference frequency is the highest freq_max across all clusters
   ref_freq_max = 2000.0

   little_procs = [platform.processor(i) for i in range(4)]
   big_procs    = [platform.processor(i) for i in range(4, 8)]

   little_sched = sim.EdfScheduler(engine, little_procs)
   big_sched    = sim.EdfScheduler(engine, big_procs)

   # Enable per-cluster reclamation
   little_sched.enable_grub()
   big_sched.enable_grub()

   little_cluster = sim.Cluster(little_clock, little_sched, 0.5, ref_freq_max)
   big_cluster    = sim.Cluster(big_clock,    big_sched,    1.0, ref_freq_max)

   # Optional utilization cap on LITTLE
   little_cluster.set_u_target(3.2)  # 80 % of 4 cores

   # The FFCapAllocator routes arrivals and creates CBS servers automatically
   allocator = sim.FFCapAllocator(engine, [little_cluster, big_cluster])

   engine.run()
