#!/usr/bin/env zsh

for dir in 65 63 61 59 57 55 53 51 49 47 45 43 41 39; do
    python scripts/find_best_ff_cap_target.py --lower-bound 0.2 --upper-bound 1.0 --max-iters 150 --total-util $dir --min-tasksets $1
done
