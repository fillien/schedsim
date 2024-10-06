#!/bin/sh

cmake --build build -t tests && ./build/tests/tests
