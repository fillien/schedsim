#!/bin/sh

clang-format -i main.cpp engine.hpp engine.cpp \
	     plateform.hpp plateform.cpp \
	     processor.hpp processor.cpp \
	     task.hpp \
	     scheduler.hpp scheduler.cpp \
	     server.cpp server.hpp \
	     event.cpp event.hpp \
	     tracer.hpp tracer.cpp \
	     trace.hpp trace.cpp
