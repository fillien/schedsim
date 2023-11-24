#!/bin/sh

clang-format -i src/main.cpp src/engine.hpp src/engine.cpp \
	     src/plateform.hpp src/plateform.cpp \
	     src/processor.hpp src/processor.cpp \
	     src/task.hpp src/event.hpp \
	     src/scheduler.hpp src/scheduler.cpp \
	     src/sched_mono.hpp src/sched_mono.cpp \
	     src/sched_parallel.hpp src/sched_parallel.cpp \
	     src/server.cpp src/server.hpp \
	     tests/main.cpp \
	     viewer/src/rtsched.cpp viewer/src/rtsched.hpp \
	     viewer/src/textual.cpp viewer/src/textual.hpp \
	     viewer/src/main.cpp \
	     traces/traces.cpp traces/traces.hpp \
	     scenario/scenario.cpp scenario/scenario.hpp \
	     generator/src/main.cpp
