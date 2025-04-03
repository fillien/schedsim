.. schedsim documentation master file, created by
   sphinx-quickstart on Thu Apr  3 16:44:13 2025.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Schedsim
========

.. toctree::
   :maxdepth: 2
   :caption: Contents:

Simulator
=========

.. doxygenfile:: entity.hpp
.. doxygenfile:: platform.hpp
.. doxygenfile:: engine.hpp
.. doxygenfile:: task.hpp
.. doxygenfile:: event.hpp
.. doxygenfile:: processor.hpp
.. doxygenfile:: server.hpp
.. doxygenfile:: timer.hpp

Allocators
==========

.. doxygenfile:: allocator.hpp
.. doxygenfile:: high_perf_first.hpp
.. doxygenfile:: low_perf_first.hpp
.. doxygenfile:: smart_ass.hpp

Schedulers
==========

.. doxygenfile:: scheduler.hpp
.. doxygenfile:: parallel.hpp
.. doxygenfile:: power_aware.hpp
.. doxygenfile:: ffa.hpp
.. doxygenfile:: csf.hpp
.. doxygenfile:: dpm_dvfs.hpp
.. doxygenfile:: csf_timer.hpp
.. doxygenfile:: ffa_timer.hpp
.. doxygenfile:: power_aware_timer.hpp

Protocols
=========

.. doxygenfile:: hardware.hpp
.. doxygenfile:: scenario.hpp
.. doxygenfile:: traces.hpp
