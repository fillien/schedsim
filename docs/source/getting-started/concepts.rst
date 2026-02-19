Core Concepts
=============

.. todo::

   Detailed concept guide will be added in a subsequent PR.

Schedsim is built around these core concepts:

**Engine**
   The event-driven simulation loop. Advances time by dispatching events
   from a priority queue.

**Platform**
   Container for all hardware resources: processor types, clock domains,
   power domains, and processors.

**Task**
   A periodic real-time task with a worst-case execution time (WCET),
   period, and relative deadline.

**Job**
   A single instance of a task, with an arrival time and actual
   execution time.

**CBS Server**
   A Constant Bandwidth Server that regulates task execution by
   enforcing a budget/period utilization bound.

**EDF Scheduler**
   Dispatches CBS servers to processors in Earliest Deadline First
   order.
