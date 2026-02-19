Schedsim
========

A suite of tools for simulating the execution of multicore real-time schedulers.

Schedsim is an event-driven discrete simulator for multicore real-time scheduling
research. It models task execution on heterogeneous processors with DVFS/DPM support,
CBS bandwidth servers, and EDF scheduling.

.. toctree::
   :maxdepth: 2
   :caption: Getting Started

   getting-started/index

.. toctree::
   :maxdepth: 3
   :caption: C++ API Reference

   api-cpp/index

.. toctree::
   :maxdepth: 2
   :caption: Tutorials

   tutorials/index

.. toctree::
   :maxdepth: 2
   :caption: Reference

   cli/index
   json-formats/index

Quick Start
-----------

1. Generate a task set:

   .. code-block:: bash

      schedgen -n 10 -u 3.9 --umax 1 --period-mode range -d 100 -o scenario.json

2. Run a simulation with GRUB reclamation:

   .. code-block:: bash

      schedsim -i scenario.json -p platforms/exynos5422LITTLE.json --reclaim grub -o trace.json

3. Analyze the trace:

   .. code-block:: bash

      schedview trace.json --energy --response-times
