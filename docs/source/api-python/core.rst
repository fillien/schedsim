Core API
========

The core module covers the simulation engine, hardware model, tasks, jobs, and
the supporting enumerations and exceptions.

.. code-block:: python

   import pyschedsim as sim

   engine = sim.Engine()
   platform = engine.platform
   cpu  = platform.add_processor_type("cpu", 1.0)
   clock = platform.add_clock_domain(500.0, 2000.0)
   power = platform.add_power_domain([(0, 0, 0.0, 100.0), (1, 0, 0.001, 10.0)])
   platform.add_processor(cpu, clock, power)

For the equivalent C++ types, see :doc:`../api-cpp/core`.

----

Enumerations
------------

ProcessorState
^^^^^^^^^^^^^^

.. py:class:: ProcessorState

   Represents the current execution state of a processor.

   .. py:attribute:: Idle

      The processor has no job assigned and is not in a low-power state.

   .. py:attribute:: ContextSwitching

      The processor is performing a context switch between jobs.

   .. py:attribute:: Running

      The processor is actively executing a job.

   .. py:attribute:: Sleep

      The processor is in a C-state (DPM sleep).

   .. py:attribute:: Changing

      The processor's clock domain is undergoing a DVFS frequency transition.

CStateScope
^^^^^^^^^^^

.. py:class:: CStateScope

   Controls whether a C-state affects a single processor or the entire power
   domain.

   .. py:attribute:: PerProcessor

      Value ``0``. The C-state is entered and exited per individual processor.

   .. py:attribute:: DomainWide

      Value ``1``. The C-state is entered only when all processors in the power
      domain are idle, and all processors wake up together.

----

Exceptions
----------

.. _AlreadyFinalizedError:

.. py:exception:: AlreadyFinalizedError

   *Inherits from* ``RuntimeError``.

   Raised when attempting to modify a platform (add processors, tasks, etc.)
   after ``finalize()`` has been called.

.. _InvalidStateError:

.. py:exception:: InvalidStateError

   *Inherits from* ``RuntimeError``.

   Raised when an operation is attempted in an invalid simulation state (e.g.,
   scheduling on an un-finalized engine).

.. _HandlerAlreadySetError:

.. py:exception:: HandlerAlreadySetError

   *Inherits from* ``RuntimeError``.

   Raised when a job-arrival handler is registered on a scheduler or allocator
   that already has a handler installed.

.. _OutOfRangeError:

.. py:exception:: OutOfRangeError

   *Inherits from* ``ValueError``.

   Raised when an index argument (e.g., ``platform.processor(idx)``) is out of
   range.

.. _AdmissionError:

.. py:exception:: AdmissionError

   *Inherits from* ``ValueError``.

   Raised when a task cannot be admitted to a scheduler because the admission
   test fails (utilization bound exceeded).

.. _LoaderError:

.. py:exception:: LoaderError

   *Inherits from* ``IOError``.

   Raised when a JSON platform or scenario file cannot be parsed or is
   structurally invalid.

----

Engine
------

.. py:class:: Engine()

   The central event-driven simulation loop. ``Engine`` owns the event priority
   queue and the ``Platform``, advancing simulation time by dispatching events
   in chronological order.

   .. rubric:: Properties

   .. py:attribute:: time
      :type: float

      Current simulation time in **seconds** (read-only).

   .. py:attribute:: platform
      :type: Platform

      Reference to the platform owned by this engine (read-only).

   .. py:attribute:: context_switch_enabled
      :type: bool

      Whether context switching is enabled (read-only).

   .. py:attribute:: energy_tracking_enabled
      :type: bool

      Whether energy tracking is enabled (read-only). Energy tracking must be
      enabled *before* ``finalize()`` is called.

   .. py:attribute:: is_finalized
      :type: bool

      Whether ``finalize()`` has been called (read-only).

   .. rubric:: Methods

   .. py:method:: run() -> None

      Run the simulation until the event queue is empty.

   .. py:method:: run(until: float) -> None

      Run until simulation time reaches *until* (seconds), then pause.

   .. py:method:: finalize() -> None

      Finalize the engine. Delegates to ``platform.finalize()`` internally.
      Must be called before creating schedulers.

   .. py:method:: schedule_job_arrival(task: Task, arrival_time: float, exec_time: float) -> None

      Schedule a single job arrival for *task* at absolute time *arrival_time*
      (seconds) with actual execution time *exec_time* (seconds).

   .. py:method:: enable_context_switch(enabled: bool) -> None

      Enable or disable context switching. Must be called before
      ``finalize()``.

   .. py:method:: enable_energy_tracking(enabled: bool) -> None

      Enable or disable energy tracking. Must be called before ``finalize()``.

   .. py:method:: processor_energy(proc_id: int) -> float

      Return the cumulative energy consumed by processor *proc_id* in **mJ**.

   .. py:method:: clock_domain_energy(cd_id: int) -> float

      Return the cumulative energy consumed by clock domain *cd_id* in **mJ**.

   .. py:method:: power_domain_energy(pd_id: int) -> float

      Return the cumulative energy consumed by power domain *pd_id* in **mJ**.

   .. py:method:: total_energy() -> float

      Return the total system energy consumed across all processors in **mJ**.

   .. py:method:: set_trace_writer(writer: TraceWriter | None) -> None

      Attach a trace writer to record simulation events. Pass ``None`` to
      detach. The writer can be set any time before ``engine.run()``.

   .. note::

      The C++ methods ``run(stop_condition)``, ``request_stop()``, and the
      property ``stop_requested`` rely on a callable stop predicate and are
      **not available** in the Python bindings. Use ``run(until)`` to stop at
      a specific time, or ``run()`` to drain the event queue.

----

Platform
--------

.. py:class:: Platform

   Container for all hardware resources: processor types, clock domains, power
   domains, processors, and tasks. Factory methods must be called before
   ``finalize()``.

   .. rubric:: Properties

   .. py:attribute:: processor_type_count
      :type: int

      Number of registered processor types (read-only).

   .. py:attribute:: processor_count
      :type: int

      Number of registered processors (read-only).

   .. py:attribute:: clock_domain_count
      :type: int

      Number of registered clock domains (read-only).

   .. py:attribute:: power_domain_count
      :type: int

      Number of registered power domains (read-only).

   .. py:attribute:: task_count
      :type: int

      Number of registered tasks (read-only).

   .. py:attribute:: reference_performance
      :type: float

      The reference performance value computed at finalization (read-only).
      Equal to the maximum ``performance`` among all processor types.

   .. py:attribute:: is_finalized
      :type: bool

      Whether ``finalize()`` has been called (read-only).

   .. rubric:: Methods

   .. py:method:: add_processor_type(name: str, performance: float, context_switch_delay: float = 0.0) -> ProcessorType

      Register a new processor type.

      :param name: Human-readable label for the processor type.
      :param performance: Relative performance (1.0 = reference speed).
      :param context_switch_delay: Context-switch overhead in **seconds**
         (default ``0.0``).
      :returns: A reference to the newly created ``ProcessorType``.

   .. py:method:: add_clock_domain(freq_min: float, freq_max: float, transition_delay: float = 0.0) -> ClockDomain

      Register a new clock domain.

      :param freq_min: Minimum frequency in **MHz**.
      :param freq_max: Maximum frequency in **MHz**.
      :param transition_delay: DVFS transition delay in **seconds**
         (default ``0.0``; set a non-zero value for realistic DVFS modelling).
      :returns: A reference to the newly created ``ClockDomain``.

   .. py:method:: add_power_domain(cstates: list[tuple[int, int, float, float]]) -> PowerDomain

      Register a new power domain.

      :param cstates: List of C-state descriptors. Each element is a
         4-tuple ``(level, scope, wake_latency_s, power_mW)`` where:

         - ``level`` (``int``) — C-state index (0 = active/C0, 1+ = sleep).
         - ``scope`` (``int``) — ``0`` for ``PerProcessor``, ``1`` for
           ``DomainWide``. Pass an integer, **not** a ``CStateScope`` enum
           value.
         - ``wake_latency_s`` (``float``) — Wake-up latency in **seconds**.
         - ``power_mW`` (``float``) — Idle power draw in **mW**.
      :returns: A reference to the newly created ``PowerDomain``.

      Example::

         power = platform.add_power_domain([
             (0, 0, 0.0,   100.0),   # C0: active, 100 mW
             (1, 0, 0.001,  10.0),   # C1: sleep, 1 ms wake, 10 mW
         ])

   .. py:method:: add_processor(type: ProcessorType, clock_domain: ClockDomain, power_domain: PowerDomain) -> Processor

      Add a processor to the platform.

      :param type: The processor type (returned by ``add_processor_type``).
      :param clock_domain: The clock domain this processor belongs to.
      :param power_domain: The power domain this processor belongs to.
      :returns: A reference to the newly created ``Processor``.

   .. py:method:: add_task(period: float, relative_deadline: float, wcet: float) -> Task

      Add a periodic task with an automatically assigned ID.

      :param period: Task period in **seconds**.
      :param relative_deadline: Relative deadline in **seconds**.
      :param wcet: Worst-case execution time in **seconds**.
      :returns: A reference to the newly created ``Task``.

   .. py:method:: add_task(id: int, period: float, relative_deadline: float, wcet: float) -> Task

      Add a periodic task with an explicit ID.

      :param id: Explicit task identifier (must be unique).
      :param period: Task period in **seconds**.
      :param relative_deadline: Relative deadline in **seconds**.
      :param wcet: Worst-case execution time in **seconds**.
      :returns: A reference to the newly created ``Task``.

   .. py:method:: finalize() -> None

      Lock the hardware configuration and compute derived values (reference
      performance, etc.). Must be called before creating schedulers.
      Raises :ref:`AlreadyFinalizedError` if called more than once.

   .. py:method:: processor_type(idx: int) -> ProcessorType

      Return the processor type at index *idx*.
      Raises :ref:`OutOfRangeError` if *idx* is out of range.

   .. py:method:: processor(idx: int) -> Processor

      Return the processor at index *idx*.
      Raises :ref:`OutOfRangeError` if *idx* is out of range.

   .. py:method:: clock_domain(idx: int) -> ClockDomain

      Return the clock domain at index *idx*.
      Raises :ref:`OutOfRangeError` if *idx* is out of range.

   .. py:method:: power_domain(idx: int) -> PowerDomain

      Return the power domain at index *idx*.
      Raises :ref:`OutOfRangeError` if *idx* is out of range.

   .. py:method:: task(idx: int) -> Task

      Return the task at index *idx*.
      Raises :ref:`OutOfRangeError` if *idx* is out of range.

----

Hardware Model
--------------

ProcessorType
^^^^^^^^^^^^^

.. py:class:: ProcessorType

   Describes a class of processors sharing the same performance and
   context-switch characteristics. Created via
   ``Platform.add_processor_type()``.

   .. py:attribute:: id
      :type: int

      Unique identifier assigned at creation (read-only).

   .. py:attribute:: performance
      :type: float

      Relative performance factor (read-only). The reference performance is the
      maximum ``performance`` value across all types in the platform.

Processor
^^^^^^^^^

.. py:class:: Processor

   A single processing unit. Created via ``Platform.add_processor()``.

   .. rubric:: Properties

   .. py:attribute:: id
      :type: int

      Unique identifier assigned at creation (read-only).

   .. py:attribute:: state
      :type: ProcessorState

      Current execution state (read-only). See ``ProcessorState``.

   .. py:attribute:: current_cstate_level
      :type: int

      Current C-state level (read-only). ``0`` means active (C0); higher values
      indicate deeper sleep states.

   .. rubric:: Methods

   .. py:method:: type() -> ProcessorType

      Return the ``ProcessorType`` of this processor.

   .. py:method:: clock_domain() -> ClockDomain

      Return the ``ClockDomain`` this processor belongs to.

   .. py:method:: power_domain() -> PowerDomain

      Return the ``PowerDomain`` this processor belongs to.

ClockDomain
^^^^^^^^^^^

.. py:class:: ClockDomain

   A group of processors sharing a voltage/frequency rail. Created via
   ``Platform.add_clock_domain()``.

   .. rubric:: Properties

   .. py:attribute:: id
      :type: int

      Unique identifier assigned at creation (read-only).

   .. py:attribute:: frequency
      :type: float

      Current operating frequency in **MHz** (read-only).

   .. py:attribute:: freq_min
      :type: float

      Minimum frequency in **MHz** (read-only).

   .. py:attribute:: freq_max
      :type: float

      Maximum frequency in **MHz** (read-only).

   .. py:attribute:: is_locked
      :type: bool

      Whether the frequency is locked (read-only). A locked clock domain
      ignores DVFS frequency-change requests.

   .. py:attribute:: is_transitioning
      :type: bool

      Whether a DVFS frequency transition is currently in progress (read-only).

   .. py:attribute:: freq_eff
      :type: float

      Effective frequency target in **MHz** (read-only). This is the frequency
      that has been requested but may not yet be reached if a transition is in
      progress.

   .. rubric:: Methods

   .. py:method:: set_freq_eff(freq: float) -> None

      Request a frequency change to *freq* (**MHz**). The actual transition
      takes ``transition_delay`` seconds if non-zero.

   .. py:method:: get_processors() -> list[Processor]

      Return all processors belonging to this clock domain.

   .. py:method:: set_power_coefficients(coeffs: list[float]) -> None

      Set polynomial coefficients for the power model
      ``P(f) = a0 + a1*f + a2*f² + a3*f³``, where *f* is in MHz and *P* is
      in mW.

      :param coeffs: A list of four coefficients ``[a0, a1, a2, a3]``.

PowerDomain
^^^^^^^^^^^

.. py:class:: PowerDomain

   A group of processors sharing power-management state (C-states). Created
   via ``Platform.add_power_domain()``.

   .. py:attribute:: id
      :type: int

      Unique identifier assigned at creation (read-only).

----

Tasks and Jobs
--------------

Task
^^^^

.. py:class:: Task

   A periodic real-time task. Created via ``Platform.add_task()``.

   .. py:attribute:: id
      :type: int

      Unique identifier (read-only).

   .. py:attribute:: period
      :type: float

      Task period in **seconds** (read-only).

   .. py:attribute:: relative_deadline
      :type: float

      Relative deadline in **seconds** (read-only).

   .. py:attribute:: wcet
      :type: float

      Worst-case execution time in **seconds** (read-only).

Job
^^^

.. py:class:: Job

   A single execution instance of a task. Jobs are created internally by the
   engine when a job-arrival event fires.

   .. rubric:: Properties

   .. py:attribute:: remaining_work
      :type: float

      Remaining execution time in **seconds** (read-only).

   .. py:attribute:: total_work
      :type: float

      Total execution time assigned to this job instance in **seconds**
      (read-only).

   .. py:attribute:: absolute_deadline
      :type: float

      Absolute deadline in **seconds** (read-only).

   .. py:attribute:: is_complete
      :type: bool

      Whether the job has finished executing (read-only).

   .. rubric:: Methods

   .. py:method:: task() -> Task

      Return the ``Task`` this job belongs to.

----

Helper Functions
----------------

.. py:function:: get_all_processors(engine: Engine) -> list[Processor]

   Return a list of all processors in the engine's platform, ordered by ID.
   Equivalent to iterating ``platform.processor(i)`` for
   ``i in range(platform.processor_count)``.

.. py:function:: get_all_tasks(engine: Engine) -> list[Task]

   Return a list of all tasks in the engine's platform, ordered by ID.
   Equivalent to iterating ``platform.task(i)`` for
   ``i in range(platform.task_count)``.
