#!/bin/sh

for i in $(seq 1 9)
do
    for mode in grub pa ffa csf
    do
        ./scripts/simulate.py data_umax0_$i $mode
    done
done
