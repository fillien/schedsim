Trace Writers and Records
=========================

Trace writers capture every simulation event and make the data available
for post-processing or persistent storage. All trace writers inherit from
the :py:class:`TraceWriter` base class and are passed to
``engine.set_trace_writer()`` before calling ``engine.run()``.

For the equivalent C++ types, see :doc:`../api-cpp/io`.

TraceWriter
-----------

.. py:class:: TraceWriter

   Abstract base class for all trace writers. Do not instantiate directly.

   A trace writer is registered with the engine via
   ``engine.set_trace_writer(writer)`` and receives a callback for every
   event that the simulation generates. Concrete subclasses decide what
   to do with those events: discard them, buffer them in memory, or stream
   them to a file.

NullTraceWriter
---------------

.. py:class:: NullTraceWriter()

   A trace writer that silently discards all events.

   Useful for benchmarking the simulation itself without the overhead of
   recording or serialising trace data.

   .. code-block:: python

      writer = sim.NullTraceWriter()
      engine.set_trace_writer(writer)
      engine.run()
      # No trace data is available after the run.

MemoryTraceWriter
-----------------

.. py:class:: MemoryTraceWriter()

   A trace writer that buffers all events in memory.

   This is the most commonly used writer for programmatic analysis. After
   the simulation completes, the buffered records are available for
   iteration and can be summarised into a :py:class:`SimulationMetrics`
   object without writing anything to disk.

   .. rubric:: Properties

   .. py:attribute:: record_count
      :type: int

      Number of events currently stored in the buffer. Read-only.

   .. rubric:: Methods

   .. py:method:: record(index: int) -> TraceRecord

      Return the trace record at position *index*.

      :param index: Zero-based index into the event buffer.
      :returns: The :py:class:`TraceRecord` at that position.
      :raises IndexError: If *index* is out of range.

   .. py:method:: clear() -> None

      Discard all buffered records and reset the event count to zero.

      Useful when running multiple simulations with the same writer
      instance without retaining data from earlier runs.

   .. py:method:: compute_metrics() -> SimulationMetrics

      Compute aggregate scheduling metrics from the buffered events.

      Returns a :py:class:`SimulationMetrics` instance populated with job
      counts, deadline misses, energy, per-task response times, and more.
      The buffer is not modified.

      :returns: A :py:class:`SimulationMetrics` summarising the trace.

   .. py:method:: track_frequency_changes() -> list[FrequencyInterval]

      Build a time-series of DVFS frequency changes from the buffered events.

      Each interval spans the period during which a cluster's frequency
      held a constant value. Intervals are non-overlapping and cover the
      full simulated time range for which frequency events were recorded.

      :returns: A list of :py:class:`FrequencyInterval` objects, one per
                contiguous constant-frequency period.

   .. py:method:: track_core_changes() -> list[CoreCountInterval]

      Build a time-series of active core count changes from the buffered
      events.

      :returns: A list of :py:class:`CoreCountInterval` objects, one per
                contiguous period with the same active core count.

   .. py:method:: track_config_changes() -> list[ConfigInterval]

      Build a time-series of combined hardware configuration changes.

      Each interval captures both the frequency and the active core count
      simultaneously, allowing analysis of how the two dimensions of
      power management interact over time.

      :returns: A list of :py:class:`ConfigInterval` objects.

FileJsonTraceWriter
-------------------

.. py:class:: FileJsonTraceWriter(filename: str)

   A trace writer that streams events as JSON to a file.

   Events are serialised incrementally as the simulation runs, so the
   output file grows continuously and memory usage does not scale with
   the number of events. The resulting file can be analysed offline with
   :py:func:`compute_metrics_from_file` or the ``schedview`` CLI tool.

   :param filename: Path to the output JSON file. The file is created or
                    overwritten when the writer is constructed.

   .. py:method:: finalize() -> None

      Close and flush the JSON file.

      **Must be called** after ``engine.run()`` completes. Failing to call
      ``finalize()`` will produce a truncated, unparseable JSON file.

   .. warning::

      Always call ``finalize()`` when using :py:class:`FileJsonTraceWriter`,
      even if an exception is raised during the simulation. Use a
      ``try``/``finally`` block to ensure cleanup:

      .. code-block:: python

         writer = sim.FileJsonTraceWriter("trace.json")
         engine.set_trace_writer(writer)
         try:
             engine.run()
         finally:
             writer.finalize()

TraceRecord
-----------

.. py:class:: TraceRecord

   A single simulation event captured by a :py:class:`MemoryTraceWriter`.

   Each record has a timestamp, a type name, and a dictionary of
   type-specific fields. The set of fields depends on the event type.

   .. rubric:: Properties

   .. py:attribute:: time
      :type: float

      Event timestamp in seconds (absolute simulation time). Read-only.

   .. py:attribute:: type
      :type: str

      Event type name string. Read-only. Common values include:

      .. list-table::
         :header-rows: 1
         :widths: 35 65

         * - Type string
           - Description
         * - ``"job_arrival"``
           - A job arrived and was submitted to the scheduler.
         * - ``"job_completion"``
           - A job finished executing on a processor.
         * - ``"job_deadline_miss"``
           - A job's absolute deadline elapsed before completion.
         * - ``"job_preemption"``
           - A running job was preempted by a higher-priority job.
         * - ``"frequency_change"``
           - A clock domain changed its operating frequency.
         * - ``"core_state_change"``
           - A processor entered or left a C-state.
         * - ``"job_rejected"``
           - A job arrival was rejected by admission control.

   .. py:attribute:: fields
      :type: dict[str, Any]

      All event-specific fields as a Python dictionary. Read-only.
      Equivalent to calling :py:meth:`get_fields_dict`.

   .. rubric:: Methods

   .. py:method:: get_field(key: str) -> Any | None

      Return the value of the field named *key*, or ``None`` if the field
      is not present in this record.

      :param key: Field name to look up.
      :returns: The field value, or ``None`` if absent.

   .. py:method:: get_fields_dict() -> dict[str, Any]

      Return all fields as a Python dictionary.

      :returns: A ``dict`` mapping field names to their values.

   .. py:method:: get_field_names() -> list[str]

      Return the names of all fields present in this record.

      :returns: A list of field name strings.

Examples
--------

Collecting and Iterating Over Events
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Use a :py:class:`MemoryTraceWriter` to buffer events and inspect them
after the simulation:

.. code-block:: python

   import pyschedsim as sim

   engine = sim.Engine()
   # ... platform setup, task injection, scheduler creation ...

   writer = sim.MemoryTraceWriter()
   engine.set_trace_writer(writer)
   engine.run()

   print(f"Recorded {writer.record_count} events")

   # Iterate over every event
   for i in range(writer.record_count):
       rec = writer.record(i)
       print(f"  t={rec.time:.6f}  type={rec.type}")

   # Filter to deadline misses only
   misses = [
       writer.record(i)
       for i in range(writer.record_count)
       if writer.record(i).type == "job_deadline_miss"
   ]
   print(f"Deadline misses: {len(misses)}")
   for rec in misses:
       task_id = rec.get_field("task_id")
       print(f"  Task {task_id} missed deadline at t={rec.time:.6f}s")

Writing a Trace File
^^^^^^^^^^^^^^^^^^^^

Stream events to a JSON file for offline analysis with ``schedview`` or
:py:func:`compute_metrics_from_file`:

.. code-block:: python

   import pyschedsim as sim

   engine = sim.Engine()
   # ... setup ...

   writer = sim.FileJsonTraceWriter("trace.json")
   engine.set_trace_writer(writer)
   try:
       engine.run()
   finally:
       writer.finalize()   # always flush and close

   # Analyse the file without loading it entirely into memory
   metrics = sim.compute_metrics_from_file("trace.json")
   print(f"Energy: {metrics.total_energy_mj:.2f} mJ")

Iterating Over TraceRecord Fields
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Inspect all fields of individual records dynamically:

.. code-block:: python

   import pyschedsim as sim

   # ... run simulation with MemoryTraceWriter ...

   for i in range(writer.record_count):
       rec = writer.record(i)
       if rec.type == "frequency_change":
           names = rec.get_field_names()
           print(f"t={rec.time:.6f}  frequency_change fields: {names}")
           for name in names:
               print(f"    {name} = {rec.get_field(name)}")
           break   # show only the first occurrence

   # Alternatively, access the fields dict directly
   rec = writer.record(0)
   for key, value in rec.fields.items():
       print(f"  {key}: {value}")

See Also
--------

- :doc:`../api-cpp/io` --- C++ ``TraceWriter``, ``MemoryTraceWriter``,
  ``JsonTraceWriter``, and ``TraceRecord`` types
- :doc:`metrics` --- computing metrics from a :py:class:`MemoryTraceWriter`
- :doc:`../tutorials/analysis` --- end-to-end trace analysis tutorial
