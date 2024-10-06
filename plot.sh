#!/bin/sh

for i in $(seq 1 9)
do
    ./scripts/energy-consumption-vs-utilization.py data_umax0_$i
    mv energy_util.png energy_util_umax0_$i.png
    mv data-energy.csv energy_umax0_$i.csv
done
