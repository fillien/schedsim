#!/usr/bin/env bash
# TODO: Rewrite to use Python API (pyschedsim)

SCEFILE=scenario.json
MallocNanoZone=0

RED="\e[31m"
ENDCOLOR="\e[0m"
# || echo -e "${RED}big_first failed${ENDCOLOR}"

while true; do
    ./build/schedgen/schedgen taskset -o $SCEFILE -t 10 -u 2 -s 1 -c 0.1 --umax 0.6
    MallocNanoZone=0 ./build/schedsim/schedsim -i $SCEFILE -p platforms/exynos5422.json --alloc big_first --sched grub
    MallocNanoZone=0 ./build/schedsim/schedsim -i $SCEFILE -p platforms/exynos5422.json --alloc little_first --sched grub
    MallocNanoZone=0 ./build/schedsim/schedsim -i $SCEFILE -p platforms/exynos5422.json --alloc smart_ass --sched grub
    if [ $? -ne 0 ]; then
        echo "Command failed. Exiting loop."
        break
    fi
done
