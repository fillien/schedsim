#!/bin/sh

clang-format -i src/main.cpp src/engine.hpp src/engine.cpp \
	     src/plateform.hpp src/plateform.cpp \
	     src/processor.hpp src/processor.cpp \
	     src/task.hpp src/event.hpp \
	     src/scheduler.hpp src/scheduler.cpp \
	     src/sched_mono.hpp src/sched_mono.cpp \
	     src/sched_parallel.hpp src/sched_parallel.cpp \
	     src/server.cpp src/server.hpp \
	     src/tracer_json.hpp src/tracer_json.cpp \
	     tests/main.cpp \
	     viewer/parse_trace.cpp viewer/parse_trace.hpp \
	     viewer/rtsched.cpp viewer/rtsched.hpp \
	     viewer/trace.hpp viewer/main.cpp
