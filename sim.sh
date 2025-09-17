#!/usr/bin/env zsh

for dir in 25; do
  for i in {1..10}; do
    ./run.sh alloc_tasksets/$dir/$i.json
  done
done
