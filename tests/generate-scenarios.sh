#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SCHEDGEN="${ROOT_DIR}/build/apps/schedgen"
OUT_DIR="${SCRIPT_DIR}/scenarios/generated"

if [ ! -x "$SCHEDGEN" ]; then
    echo "ERROR: schedgen not found at $SCHEDGEN — build first" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

count=0
for util_x100 in $(seq 50 5 550); do
    util=$(echo "scale=2; $util_x100 / 100" | bc)
    for rep in $(seq 1 10); do
        count=$((count + 1))
        fname="u${util}_${rep}.json"
        "$SCHEDGEN" taskset -t 10 -u "$util" -m 1.0 -n 0.01 -s 1.0 -c 1.0 -o "${OUT_DIR}/${fname}"
    done
done
echo "Generated $count scenarios in $OUT_DIR"
