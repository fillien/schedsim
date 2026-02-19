C++ API Overview
================

Schedsim's C++ API is split into three libraries:

**Core** (``schedsim::core``)
   The simulation engine, hardware model (processors, clock domains, power
   domains), tasks, jobs, timers, deferred callbacks, and strong types
   (Duration, TimePoint, Frequency, Power, Energy). Has no external
   dependencies beyond the standard library.

**Algo** (``schedsim::algo``)
   Scheduling algorithms built on top of core. Includes the EDF scheduler,
   CBS bandwidth servers, GRUB/CASH reclamation policies, DVFS frequency
   scaling (PA, FFA, CSF), DPM power management, and multi-cluster task
   allocators.

**I/O** (``schedsim::io``)
   External data handling: JSON platform and scenario loaders, trace writers
   (JSON, textual, in-memory, null), post-simulation metrics, and random
   scenario generation.

.. toctree::
   :maxdepth: 2

   core
   algo
   io
