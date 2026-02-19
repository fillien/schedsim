Core Library
============

The core library (``schedsim::core``) provides the simulation engine, hardware
model, and foundational types. It has no dependencies on scheduling algorithms
or I/O formats.

Types
-----

The core type system uses strong wrappers for physical quantities.
``Duration`` and ``TimePoint`` store nanosecond-precision values as ``int64_t``;
construction goes through named bridge functions rather than public constructors.

Duration
^^^^^^^^

.. doxygenclass:: schedsim::core::Duration
   :members:

TimePoint
^^^^^^^^^

.. doxygenclass:: schedsim::core::TimePoint
   :members:

Bridge Functions
^^^^^^^^^^^^^^^^

.. doxygenfunction:: schedsim::core::duration_from_seconds
.. doxygenfunction:: schedsim::core::duration_from_seconds_ceil
.. doxygenfunction:: schedsim::core::duration_to_seconds
.. doxygenfunction:: schedsim::core::duration_from_nanoseconds
.. doxygenfunction:: schedsim::core::duration_to_nanoseconds
.. doxygenfunction:: schedsim::core::time_from_seconds
.. doxygenfunction:: schedsim::core::time_to_seconds
.. doxygenfunction:: schedsim::core::scale_duration
.. doxygenfunction:: schedsim::core::divide_duration
.. doxygenfunction:: schedsim::core::duration_ratio

Frequency
^^^^^^^^^

.. doxygenstruct:: schedsim::core::Frequency
   :members:

Power
^^^^^

.. doxygenstruct:: schedsim::core::Power
   :members:

Energy
^^^^^^

.. doxygenstruct:: schedsim::core::Energy
   :members:

Engine
------

The Engine is the central event-driven simulation loop. It owns the event
priority queue and the Platform, advancing simulation time by dispatching
events in chronological order.

.. doxygenclass:: schedsim::core::Engine
   :members:

Platform
--------

The Platform is the container for all hardware resources: processor types,
clock domains, power domains, processors, and tasks. Factory methods must
be called before ``Engine::finalize()``.

.. doxygenclass:: schedsim::core::Platform
   :members:

Hardware Model
--------------

ProcessorType
^^^^^^^^^^^^^

.. doxygenclass:: schedsim::core::ProcessorType
   :members:

Processor
^^^^^^^^^

.. doxygenenum:: schedsim::core::ProcessorState

.. doxygenclass:: schedsim::core::Processor
   :members:

ClockDomain
^^^^^^^^^^^^

.. doxygenclass:: schedsim::core::ClockDomain
   :members:

PowerDomain
^^^^^^^^^^^^

.. doxygenenum:: schedsim::core::CStateScope

.. doxygenstruct:: schedsim::core::CStateLevel
   :members:

.. doxygenclass:: schedsim::core::PowerDomain
   :members:

Tasks and Jobs
--------------

Task
^^^^

.. doxygenclass:: schedsim::core::Task
   :members:

Job
^^^

.. doxygenclass:: schedsim::core::Job
   :members:

Events and Timers
-----------------

EventKey
^^^^^^^^

.. doxygenstruct:: schedsim::core::EventKey
   :members:

EventPriority
^^^^^^^^^^^^^

.. doxygenstruct:: schedsim::core::EventPriority
   :members:

Event Types
^^^^^^^^^^^

.. doxygenstruct:: schedsim::core::JobArrivalEvent
   :members:

.. doxygenstruct:: schedsim::core::JobCompletionEvent
   :members:

.. doxygenstruct:: schedsim::core::DeadlineMissEvent
   :members:

.. doxygenstruct:: schedsim::core::ProcessorAvailableEvent
   :members:

.. doxygenstruct:: schedsim::core::TimerEvent
   :members:

TimerId
^^^^^^^

.. doxygenclass:: schedsim::core::TimerId
   :members:

DeferredId
^^^^^^^^^^

.. doxygenclass:: schedsim::core::DeferredId
   :members:

Trace Writer
------------

.. doxygenclass:: schedsim::core::TraceWriter
   :members:

Energy Tracker
--------------

.. doxygenclass:: schedsim::core::EnergyTracker
   :members:

Errors
------

.. doxygenclass:: schedsim::core::SimulationError
   :members:

.. doxygenclass:: schedsim::core::InvalidStateError
   :members:

.. doxygenclass:: schedsim::core::OutOfRangeError
   :members:

.. doxygenclass:: schedsim::core::AlreadyFinalizedError
   :members:

.. doxygenclass:: schedsim::core::HandlerAlreadySetError
   :members:
