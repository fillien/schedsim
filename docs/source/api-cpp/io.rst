I/O Library
===========

The I/O library (``schedsim::io``) handles JSON loading, trace output,
scenario generation, and post-simulation metrics computation.

Platform Loading
----------------

.. doxygenfunction:: schedsim::io::load_platform

.. doxygenfunction:: schedsim::io::load_platform_from_string

Scenario Loading
----------------

Data Structures
^^^^^^^^^^^^^^^

.. doxygenstruct:: schedsim::io::JobParams
   :members:

.. doxygenstruct:: schedsim::io::TaskParams
   :members:

.. doxygenstruct:: schedsim::io::ScenarioData
   :members:

Loading Functions
^^^^^^^^^^^^^^^^^

.. doxygenfunction:: schedsim::io::load_scenario

.. doxygenfunction:: schedsim::io::load_scenario_from_string

Writing Functions
^^^^^^^^^^^^^^^^^

.. doxygenfunction:: schedsim::io::write_scenario

.. doxygenfunction:: schedsim::io::write_scenario_to_stream

Scenario Injection
------------------

.. doxygenfunction:: schedsim::io::inject_scenario

.. doxygenfunction:: schedsim::io::schedule_arrivals

Scenario Generation
-------------------

.. doxygenfunction:: schedsim::io::generate_scenario

.. doxygenfunction:: schedsim::io::generate_task_set

.. doxygenfunction:: schedsim::io::generate_arrivals

.. doxygenfunction:: schedsim::io::uunifast

.. doxygenfunction:: schedsim::io::uunifast_discard

.. doxygenfunction:: schedsim::io::generate_weibull_jobs

.. doxygenfunction:: schedsim::io::pick_harmonic_period

.. doxygenfunction:: schedsim::io::generate_uunifast_discard_weibull

.. doxygenfunction:: schedsim::io::merge_scenarios

.. doxygenfunction:: schedsim::io::from_utilizations

.. doxygenstruct:: schedsim::io::PeriodDistribution
   :members:

.. doxygenstruct:: schedsim::io::WeibullJobConfig
   :members:

Trace Writers
-------------

.. doxygenstruct:: schedsim::io::TraceRecord
   :members:

.. doxygenclass:: schedsim::io::NullTraceWriter
   :members:

.. doxygenclass:: schedsim::io::JsonTraceWriter
   :members:

.. doxygenclass:: schedsim::io::MemoryTraceWriter
   :members:

.. doxygenclass:: schedsim::io::TextualTraceWriter
   :members:

Metrics
-------

.. doxygenstruct:: schedsim::io::SimulationMetrics
   :members:

.. doxygenstruct:: schedsim::io::ResponseTimeStats
   :members:

.. doxygenstruct:: schedsim::io::FrequencyInterval
   :members:

.. doxygenstruct:: schedsim::io::CoreCountInterval
   :members:

.. doxygenstruct:: schedsim::io::ConfigInterval
   :members:

.. doxygenfunction:: schedsim::io::compute_metrics

.. doxygenfunction:: schedsim::io::compute_metrics_from_file

.. doxygenfunction:: schedsim::io::compute_response_time_stats

.. doxygenfunction:: schedsim::io::track_frequency_changes

.. doxygenfunction:: schedsim::io::track_core_changes

.. doxygenfunction:: schedsim::io::track_config_changes

Errors
------

.. doxygenclass:: schedsim::io::LoaderError
   :members:
