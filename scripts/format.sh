#!/bin/sh

clang-format -i \
	     protocols/scenorio/scenario.cpp \
	     protocols/traces/traces.cpp \
	     schedgen/src/main.cpp \
	     schedgen/src/task_generator.cpp \
	     schedsim/src/engine.cpp \
	     schedsim/src/main.cpp \
	     schedsim/src/plateform.cpp \
	     schedsim/src/processor.cpp \
	     schedsim/src/sched_mono.cpp \
	     schedsim/src/sched_parallel.cpp \
	     schedsim/src/scheduler.cpp \
	     schedsim/src/server.cpp \
	     schedsim/src/task.cpp \
	     schedview/src/core_utilization.cpp \
	     schedview/src/deadline_misses.cpp \
	     schedview/src/energy.cpp \
	     schedview/src/main.cpp \
	     schedview/src/rtsched.cpp \
	     schedview/src/stats.cpp \
	     schedview/src/textual.cpp \
	     tests/main.cpp \
	     protocols/scenario/scenario.hpp \
	     protocols/traces/traces.hpp \
	     schedgen/src/task_generator.hpp \
	     schedsim/src/engine.hpp \
	     schedsim/src/entity.hpp \
	     schedsim/src/event.hpp \
	     schedsim/src/plateform.hpp \
	     schedsim/src/processor.hpp \
	     schedsim/src/sched_mono.hpp \
	     schedsim/src/sched_parallel.hpp \
	     schedsim/src/scheduler.hpp \
	     schedsim/src/server.hpp \
	     schedsim/src/task.hpp \
	     schedview/src/core_utilization.hpp \
	     schedview/src/deadline_misses.hpp \
	     schedview/src/energy.hpp \
	     schedview/src/rtsched.hpp \
	     schedview/src/stats.hpp \
	     schedview/src/textual.hpp
