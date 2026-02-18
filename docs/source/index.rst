.. schedsim documentation master file, created by
   sphinx-quickstart on Thu Apr  3 16:44:13 2025.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

.. toctree::
    :maxdepth: 3
    :hidden:

    self
    api-cpp/api
    api-python

Schedsim
========
A suite of tools for simulating the execution of multicore real-time schedulers.

Installation
============

To compile the software, you can use `Nix <https://nixos.org>`_:

.. code-block:: bash

    nix build

Then run the programs:

.. code-block:: bash

    $ result/bin/schedgen
    $ result/bin/schedsim
    $ result/bin/schedview
    $ result/bin/alloc

To compile the software using ``cmake``:

.. code-block:: bash

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

Then run the programs:

.. code-block:: bash

    $ build/apps/schedgen
    $ build/apps/schedsim
    $ build/apps/schedview
    $ build/apps/alloc

Usage
=====

1. Use ``schedgen`` to generate new task sets with your parameters. Here, we
   generate a scenario of 10 tasks with a total utilization of 3.9, maximum
   per-task utilization of 1, and a simulation duration of 100 seconds.

   .. code-block:: bash

       ./build/apps/schedgen -n 10 -u 3.9 --umax 1 --period-mode range -d 100 -o scenario.json

2. Then, use ``schedsim`` to simulate the execution of your task set and specify a platform for it to run on.

   .. note::
       You can use the platforms provided in the ``platforms/`` folder.

   In the following example, we use the LITTLE cluster of the exynos5422 (composed of four cores) to run the previously generated task set, and we schedule it with GRUB reclamation.

   .. code-block:: bash

       ./build/apps/schedsim -i scenario.json -p platforms/exynos5422LITTLE.json --reclaim grub -o trace.json

3. Finally, use ``schedview`` to analyze the execution traces (in JSON) and compute metrics.

   .. code-block:: bash

       ./build/apps/schedview trace.json --energy --response-times

In the following sections you'll find the options available for each command.

Schedgen
--------

Task set generator (UUniFast-Discard with configurable period distributions).

Usage: ``schedgen [OPTION...]``

- ``-n, --tasks arg``        Number of tasks (required).
- ``-u, --utilization arg``  Target total utilization (required, can exceed 1.0 for multicore).
- ``--umin arg``             Min per-task utilization [0,1] (default: 0).
- ``--umax arg``             Max per-task utilization [0,1] (default: 1).
- ``-s, --success arg``      Success rate for deadline budget [0,1] (default: 1).
- ``-c, --compression arg``  Compression ratio (min duration/WCET) [0,1] (default: 1).
- ``--period-mode arg``      Period selection: 'harmonic' or 'range' (default: range).
- ``--period-min arg``       Min period in ms (default: 10, range mode only).
- ``--period-max arg``       Max period in ms (default: 1000, range mode only).
- ``-d, --duration arg``     Simulation duration in seconds (range mode only).
- ``--seed arg``             Random seed.
- ``--batch arg``            Generate multiple scenarios.
- ``--dir arg``              Output directory for batch mode.
- ``-o, --output arg``       Output file (default: stdout).
- ``-h, --help``             Show help.

Schedsim
--------

Real-time scheduler simulator.

Usage: ``schedsim [OPTION...]``

- ``-i, --input arg``       Scenario file, JSON (required).
- ``-p, --platform arg``    Platform configuration, JSON (required).
- ``--reclaim arg``          Reclamation: none, grub, cash (default: none).
- ``--dvfs arg``             DVFS: none, power-aware, ffa, csf (default: none).
- ``--dvfs-cooldown arg``    DVFS cooldown in ms (default: 0).
- ``--dpm arg``              DPM: none, basic (default: none).
- ``--dpm-cstate arg``       Target C-state (default: 1).
- ``-d, --duration arg``     Simulation duration in seconds (default: auto).
- ``--energy``               Enable energy tracking.
- ``--context-switch``       Enable context switch overhead.
- ``--format arg``           Output format: json, null (default: json).
- ``--metrics``              Print metrics to stderr.
- ``-o, --output arg``       Trace output file (default: stdout).
- ``-h, --help``             Show help.

Schedview
---------

Trace analyzer for post-simulation analysis.

Usage: ``schedview [OPTION...] <trace-file>``

- ``trace-file``             JSON trace file (positional).
- ``-d, --directory arg``    Process all *.json traces in directory.
- ``--deadline-misses``      Show deadline miss details.
- ``--response-times``       Show response time statistics.
- ``--energy``               Show energy breakdown.
- ``--format arg``           Output format: text, csv, json (default: text).
- ``-h, --help``             Show help.

Alloc
-----

Multi-cluster allocator testing tool.

Usage: ``alloc [OPTION...]``

- ``-i, --input arg``       Scenario file (required).
- ``-p, --platform arg``    Platform configuration (required).
- ``-a, --alloc arg``       Allocator name (required).
- ``-g, --granularity arg`` Granularity: per-cluster or per-core (default: per-cluster).
- ``-A, --alloc-arg arg``   Allocator argument key=value (repeatable).
- ``--target arg``           u_target for LITTLE clusters.
- ``--reclaim arg``          Reclamation: none, grub, cash (default: none).
- ``--dvfs arg``             DVFS: none, power-aware, ffa, csf, ffa-timer, csf-timer (default: none).
- ``-h, --help``             Show help.
