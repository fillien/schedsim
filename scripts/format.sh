#!/bin/sh

clang-format -i src/main.cpp src/engine.hpp src/engine.cpp \
	     src/plateform.hpp src/plateform.cpp \
	     src/processor.hpp src/processor.cpp \
	     src/task.hpp \
	     src/scheduler.hpp src/scheduler.cpp \
	     src/sched_mono.hpp src/sched_mono.cpp \
	     src/sched_parallel.hpp src/sched_parallel.cpp \
	     src/server.cpp src/server.hpp \
	     src/event.cpp src/event.hpp \
	     src/tracer.hpp src/tracer.cpp
