JSON Formats
============

Schedsim uses two JSON formats: **Platform** files describe the hardware, and
**Scenario** files describe the workload.

Units
-----

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Quantity
     - Unit
     - Notes
   * - Time (scenario)
     - seconds
     - Periods, deadlines, WCET, job arrivals and durations
   * - Frequency
     - MHz
     - Clock domain frequency values
   * - Power
     - mW
     - C-state idle power, power model output
   * - Latency / Delay
     - microseconds
     - Transition delays, context switch delays, C-state wake latency


Platform JSON
-------------

Describes the hardware: processor types, clock domains with DVFS frequency
tables, power domains with C-state levels, and processor assignments.

Format Detection
^^^^^^^^^^^^^^^^

The loader auto-detects the format based on top-level keys:

- If the root object contains ``"clusters"``, the **legacy format** is used.
- Otherwise, the **new (component-based) format** is assumed.

New Format (Component-Based)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The new format defines hardware components separately and references them by
ID or name when creating processors.

**processor_types**

.. list-table::
   :header-rows: 1
   :widths: 25 10 10 10 45

   * - Field
     - Type
     - Required
     - Default
     - Description
   * - ``name``
     - string
     - yes
     -
     - Unique identifier for this processor type
   * - ``performance``
     - number
     - yes
     -
     - Relative performance (1.0 = reference speed)
   * - ``context_switch_delay_us``
     - number
     - no
     - ``0``
     - Context switch overhead in microseconds

**clock_domains**

.. list-table::
   :header-rows: 1
   :widths: 25 10 10 10 45

   * - Field
     - Type
     - Required
     - Default
     - Description
   * - ``id``
     - integer
     - yes
     -
     - Unique domain identifier
   * - ``frequencies_mhz``
     - array
     - yes
     -
     - Available frequencies in MHz, sorted ascending. First element is
       freq_min, last is freq_max.
   * - ``transition_delay_us``
     - number
     - no
     - ``0``
     - DVFS transition delay in microseconds
   * - ``initial_frequency_mhz``
     - number
     - no
     - freq_max
     - Starting frequency. Defaults to the highest frequency in the array.
   * - ``effective_frequency_mhz``
     - number
     - no
     -
     - Effective frequency used for DVFS utilization scaling calculations
   * - ``power_model``
     - array[4]
     - no
     -
     - Polynomial coefficients ``[a0, a1, a2, a3]`` where
       ``P(f) = a0 + a1*f + a2*f^2 + a3*f^3`` (P in mW, f in MHz)

**power_domains**

.. list-table::
   :header-rows: 1
   :widths: 25 10 10 10 45

   * - Field
     - Type
     - Required
     - Default
     - Description
   * - ``id``
     - integer
     - yes
     -
     - Unique domain identifier
   * - ``c_states``
     - array
     - no
     - C0 only
     - List of C-state level objects. If omitted, a default C0-active state
       is created.

Each C-state object:

.. list-table::
   :header-rows: 1
   :widths: 25 10 10 10 45

   * - Field
     - Type
     - Required
     - Default
     - Description
   * - ``level``
     - integer
     - yes
     -
     - C-state level number (0 = active, higher = deeper sleep)
   * - ``power_mw``
     - number
     - yes
     -
     - Idle power consumption in milliwatts
   * - ``latency_us``
     - number
     - no
     - ``0``
     - Wake-up latency in microseconds
   * - ``scope``
     - string
     - no
     - ``"per_processor"``
     - ``"per_processor"`` or ``"domain_wide"``

**processors**

.. list-table::
   :header-rows: 1
   :widths: 25 10 10 10 45

   * - Field
     - Type
     - Required
     - Default
     - Description
   * - ``type``
     - string
     - yes
     -
     - Name of a defined processor type
   * - ``clock_domain``
     - integer
     - yes
     -
     - ID of a defined clock domain
   * - ``power_domain``
     - integer
     - yes
     -
     - ID of a defined power domain

**Full example** (new format):

.. code-block:: json

   {
     "processor_types": [
       {"name": "big",    "performance": 1.0},
       {"name": "LITTLE", "performance": 0.3334, "context_switch_delay_us": 5}
     ],
     "clock_domains": [
       {
         "id": 0,
         "frequencies_mhz": [200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800, 2000, 2100],
         "transition_delay_us": 50,
         "effective_frequency_mhz": 1200,
         "power_model": [0.0443, 3.41e-6, 2.19e-8, 4.61e-11]
       },
       {
         "id": 1,
         "frequencies_mhz": [200, 400, 600, 800, 1000, 1200, 1300, 1400],
         "transition_delay_us": 50,
         "effective_frequency_mhz": 600,
         "power_model": [0.0443, 3.41e-6, 2.19e-8, 4.61e-11]
       }
     ],
     "power_domains": [
       {
         "id": 0,
         "c_states": [
           {"level": 0, "power_mw": 100, "scope": "per_processor"},
           {"level": 1, "power_mw": 10, "latency_us": 100, "scope": "per_processor"}
         ]
       },
       {"id": 1}
     ],
     "processors": [
       {"type": "big",    "clock_domain": 0, "power_domain": 0},
       {"type": "big",    "clock_domain": 0, "power_domain": 0},
       {"type": "big",    "clock_domain": 0, "power_domain": 0},
       {"type": "big",    "clock_domain": 0, "power_domain": 0},
       {"type": "LITTLE", "clock_domain": 1, "power_domain": 1},
       {"type": "LITTLE", "clock_domain": 1, "power_domain": 1},
       {"type": "LITTLE", "clock_domain": 1, "power_domain": 1},
       {"type": "LITTLE", "clock_domain": 1, "power_domain": 1}
     ]
   }

Legacy Format (Cluster-Based)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The legacy format groups processors into clusters. Each cluster implicitly
defines a processor type, clock domain, and power domain.

.. list-table::
   :header-rows: 1
   :widths: 25 10 10 10 45

   * - Field
     - Type
     - Required
     - Default
     - Description
   * - ``procs``
     - integer
     - yes
     -
     - Number of processors in this cluster
   * - ``perf_score``
     - number
     - no
     - ``1.0``
     - Relative performance (1.0 = reference speed)
   * - ``effective_freq``
     - number
     - no
     - ``1000.0``
     - Effective frequency in MHz for utilization scaling
   * - ``frequencies``
     - array
     - no
     -
     - Available frequencies in MHz, sorted ascending. If omitted, a
       single-frequency domain is created at ``effective_freq``.
   * - ``power_model``
     - array[4]
     - no
     -
     - Power model coefficients (same as new format)

Legacy format notes:

- Processor type names are auto-generated as ``cluster0``, ``cluster1``, etc.
- The clock domain initialises at freq_max (highest frequency in the array).
- A default C0-only power domain is created for each cluster.

**Legacy format example** (Exynos 5422 big.LITTLE):

.. code-block:: json

   {
     "clusters": [
       {
         "procs": 4,
         "perf_score": 1.0,
         "effective_freq": 1200.0,
         "frequencies": [200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800, 2000, 2100],
         "power_model": [0.0443, 3.41e-6, 2.19e-8, 4.61e-11]
       },
       {
         "procs": 4,
         "perf_score": 0.3334,
         "effective_freq": 600.0,
         "frequencies": [200, 400, 600, 800, 1000, 1200, 1300, 1400],
         "power_model": [0.0443, 3.41e-6, 2.19e-8, 4.61e-11]
       }
     ]
   }


Scenario JSON
-------------

Describes the workload: a set of periodic real-time tasks with optional
concrete job arrivals.

**tasks**

.. list-table::
   :header-rows: 1
   :widths: 25 10 10 10 45

   * - Field
     - Type
     - Required
     - Default
     - Description
   * - ``id``
     - integer
     - yes
     -
     - Unique task identifier (1-based by convention)
   * - ``period``
     - number
     - yes
     -
     - Inter-arrival period in seconds. Must be positive.
   * - ``relative_deadline``
     - number
     - no
     - ``period``
     - Relative deadline in seconds
   * - ``wcet``
     - number
     - \*
     -
     - Worst-case execution time in seconds. Must be positive.
   * - ``utilization``
     - number
     - \*
     -
     - Per-task utilization in (0, 1]. WCET is computed as
       ``period * utilization``.
   * - ``jobs``
     - array
     - no
     -
     - Concrete job instances (see below)

\* Exactly one of ``wcet`` or ``utilization`` must be specified. The canonical
output format always uses ``wcet``.

Each job object:

.. list-table::
   :header-rows: 1
   :widths: 25 10 10 10 45

   * - Field
     - Type
     - Required
     - Default
     - Description
   * - ``arrival``
     - number
     - yes
     -
     - Absolute arrival time in seconds
   * - ``duration``
     - number
     - yes
     -
     - Actual execution demand in seconds. Must be positive.

Validation Rules
^^^^^^^^^^^^^^^^

- ``period > 0``
- Either ``wcet > 0`` or ``utilization`` in (0, 1] must be specified
- ``relative_deadline >= wcet`` (checked after WCET is resolved)
- ``duration > 0`` for all jobs
- Jobs are sorted by arrival time after loading (input order does not matter)
- ``duration`` may exceed ``wcet`` --- this is not enforced by the loader

**Scenario example:**

.. code-block:: json

   {
     "tasks": [
       {
         "id": 1,
         "period": 0.010,
         "relative_deadline": 0.010,
         "wcet": 0.003,
         "jobs": [
           {"arrival": 0.0,   "duration": 0.002},
           {"arrival": 0.010, "duration": 0.003},
           {"arrival": 0.020, "duration": 0.0025}
         ]
       },
       {
         "id": 2,
         "period": 0.020,
         "wcet": 0.005,
         "jobs": [
           {"arrival": 0.0,   "duration": 0.004},
           {"arrival": 0.020, "duration": 0.0045}
         ]
       }
     ]
   }

In this example, task 2 omits ``relative_deadline``, so it defaults to the
period (implicit deadline). Both tasks specify ``wcet`` directly.
