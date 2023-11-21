#!/bin/sh

cmake --build build -t launch && cmake --build build -t viewer && ./build/src/launch "$1" && ./build/viewer/viewer -o mydessin.tex out.json
