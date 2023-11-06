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
	     viewer/src/json.cpp viewer/src/json.hpp \
	     viewer/src/rtsched.cpp viewer/src/rtsched.hpp \
	     viewer/src/textual.cpp viewer/src/textual.hpp \
	     viewer/src/trace.hpp viewer/src/main.cpp
