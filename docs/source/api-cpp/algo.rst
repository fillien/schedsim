Algo Library
============

The algo library (``schedsim::algo``) implements scheduling algorithms,
bandwidth servers, allocators, and energy management policies on top of the
core simulation engine.

Schedulers
----------

Scheduler (Abstract Base)
^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::Scheduler
   :members:

EDF Scheduler
^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::EdfScheduler
   :members:

CBS Servers
-----------

.. doxygenclass:: schedsim::algo::CbsServer
   :members:

Allocators
----------

Allocator (Abstract Base)
^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::Allocator
   :members:

Cluster
^^^^^^^

.. doxygenclass:: schedsim::algo::Cluster
   :members:

MultiClusterAllocator
^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::MultiClusterAllocator
   :members:

SingleSchedulerAllocator
^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::SingleSchedulerAllocator
   :members:

FirstFitAllocator
^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::FirstFitAllocator
   :members:

WorstFitAllocator
^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::WorstFitAllocator
   :members:

BestFitAllocator
^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::BestFitAllocator
   :members:

FFBigFirstAllocator
^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::FFBigFirstAllocator
   :members:

FFLittleFirstAllocator
^^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::FFLittleFirstAllocator
   :members:

FFCapAllocator
^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::FFCapAllocator
   :members:

FFCapAdaptiveLinearAllocator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::FFCapAdaptiveLinearAllocator
   :members:

FFCapAdaptivePolyAllocator
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::FFCapAdaptivePolyAllocator
   :members:

FFLbAllocator
^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::FFLbAllocator
   :members:

CountingAllocator
^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::CountingAllocator
   :members:

MCTSAllocator
^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::MCTSAllocator
   :members:

Reclamation Policies
--------------------

ReclamationPolicy (Abstract Base)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::ReclamationPolicy
   :members:

GrubPolicy
^^^^^^^^^^

.. doxygenclass:: schedsim::algo::GrubPolicy
   :members:

CashPolicy
^^^^^^^^^^

.. doxygenclass:: schedsim::algo::CashPolicy
   :members:

DVFS Policies
-------------

DvfsPolicy (Abstract Base)
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::DvfsPolicy
   :members:

CooldownTimer
^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::CooldownTimer
   :members:

PowerAwareDvfsPolicy
^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::PowerAwareDvfsPolicy
   :members:

FfaPolicy
^^^^^^^^^

.. doxygenclass:: schedsim::algo::FfaPolicy
   :members:

CsfPolicy
^^^^^^^^^

.. doxygenclass:: schedsim::algo::CsfPolicy
   :members:

FfaTimerPolicy
^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::FfaTimerPolicy
   :members:

CsfTimerPolicy
^^^^^^^^^^^^^^

.. doxygenclass:: schedsim::algo::CsfTimerPolicy
   :members:

DPM Policy
----------

.. doxygenclass:: schedsim::algo::DpmPolicy
   :members:

.. doxygenclass:: schedsim::algo::BasicDpmPolicy
   :members:

Utilities
---------

DVFS/DPM Utilities
^^^^^^^^^^^^^^^^^^

.. doxygennamespace:: schedsim::algo::dvfs_dpm
   :members:

Task Utilities
^^^^^^^^^^^^^^

.. doxygenfunction:: schedsim::algo::task_utilization

Errors
------

.. doxygenenum:: schedsim::algo::AdmissionTest

.. doxygenenum:: schedsim::algo::DeadlineMissPolicy

.. doxygenclass:: schedsim::algo::AdmissionError
   :members:
