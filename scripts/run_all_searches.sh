#!/usr/bin/env bash
set -euo pipefail

UTILS=(21 23 25 27 29 31 33 35 37 39 41 43 45 47 49 51 53 55 57 59 61 63 65)

for d in min_tasksets_big_task min_tasksets_little_task min_tasksets_mid_task; do
    echo "=== Processing $d ==="
    for u in "${UTILS[@]}"; do
        # Skip if taskset folder doesn't exist
        if [ ! -d "$d/$u" ]; then
            echo "  Skipping U=$u (no taskset folder)"
            continue
        fi
        # Skip if CSV already exists
        if [ -f "$d/ff_cap_search_${u}.csv" ]; then
            echo "  Skipping U=$u (CSV already exists)"
            continue
        fi
        echo -n "  U=$u ... "
        python scripts/find_best_ff_cap_target.py \
            --total-util "$u" \
            --min-tasksets "$d" \
            --lower-bound 0.0 \
            --upper-bound 1.0 \
            --initial-step 0.02 \
            --min-step 0.0005 \
            > /dev/null 2>&1
        mv "ff_cap_search_${u}.csv" "$d/"
        echo "done"
    done
    echo
done

echo "All searches complete."
