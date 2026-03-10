Python API Overview
===================

The ``pyschedsim`` module provides Python bindings for the Schedsim C++
libraries via `nanobind <https://nanobind.readthedocs.io/>`_.

Installation
------------

Build with the CMake ``-DBUILD_PYTHON=ON`` flag:

.. code-block:: bash

   cmake -B build -DBUILD_PYTHON=ON
   cmake --build build

Alternatively, use the provided Nix flake, which enables the Python bindings
by default:

.. code-block:: bash

   nix develop
   # pyschedsim is available in the environment

Import Convention
-----------------

.. code-block:: python

   import pyschedsim as sim

All classes and functions are in the flat ``pyschedsim`` namespace — there are
no sub-modules.

Unit Conventions
----------------

Python scalars are automatically converted to and from the C++ strong types.
The following table lists the units expected by each Python-facing type:

.. list-table::
   :header-rows: 1
   :widths: 20 20 20 40

   * - Python type
     - C++ type
     - Unit
     - Notes
   * - ``float``
     - ``Duration``
     - seconds
     - Time intervals (period, WCET, exec_time, …)
   * - ``float``
     - ``TimePoint``
     - seconds
     - Absolute simulation time
   * - ``float``
     - ``Frequency``
     - MHz
     - Clock domain frequencies
   * - ``float``
     - ``Power``
     - mW
     - milliwatts
   * - ``float``
     - ``Energy``
     - mJ
     - millijoules

Integer arguments are automatically accepted wherever a float is expected.

.. warning::

   Frequency values in ``ClockDomain`` properties use **MHz**, but the
   ``frequency`` field of ``FrequencyInterval`` and ``ConfigInterval`` (from
   trace analysis) may use raw trace values. Check the units carefully when
   mixing trace data with platform configuration.

Module Structure
----------------

All public symbols are exported directly under ``pyschedsim``:

- **Core**: ``Engine``, ``Platform``, ``Processor``, ``ProcessorType``,
  ``ClockDomain``, ``PowerDomain``, ``Task``, ``Job``, ``ProcessorState``,
  ``CStateScope``, helper functions
- **Scheduling**: ``EdfScheduler``, ``CbsServer``, ``Cluster``,
  ``SingleSchedulerAllocator``, ``FFCapAllocator``, ``CountingAllocator``,
  ``AdmissionTest``, ``DeadlineMissPolicy``
- **I/O**: ``TraceWriter``, ``MemoryTraceWriter``, ``FileJsonTraceWriter``,
  ``NullTraceWriter``, platform and scenario loaders
- **Tracing / metrics**: ``SimulationMetrics``, trace record types

For the equivalent C++ API, see :doc:`../api-cpp/index`.

.. toctree::
   :maxdepth: 2

   core
   scheduling
   io
   tracing
   metrics
