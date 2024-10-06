#!/bin/sh

for i in $(seq 1 9)
do
    for mode in grub pa pa_f_min pa_m_min
    do
        ./scripts/simulate.py data_umax0_$i $mode
    done
done
