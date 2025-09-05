#!/usr/bin/env zsh

for dir in 33 31; do
  for i in {1..10}; do
    ./run.sh alloc_tasksets/$dir/$i.json
  done
done
