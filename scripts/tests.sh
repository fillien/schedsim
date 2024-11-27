#!/usr/bin/env bash

while true; do
  echo "new"
  ./build/schedgen/schedgen taskset -t 10 -u 3 -s 1 -c 1 -m 1  &&  ./build/schedsim/schedsim -s scenario.json --sched csf --delay -p platforms/exynos5422LITTLE.json
  if [ $? -ne 0 ]; then
    echo "Command failed. Exiting loop."
    break
  fi
done
