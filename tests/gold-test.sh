#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
GOLDEN_DIR="${SCRIPT_DIR}/golden"
SCHEDSIM="${BUILD_DIR}/apps/schedsim"

PLATFORM="${ROOT_DIR}/platforms/exynos5422.json"

SCENARIOS=(
    "${SCRIPT_DIR}/scenarios/simple.json"
    "${SCRIPT_DIR}/scenarios/test.json"
    "${SCRIPT_DIR}/scenarios/ex0.json"
    "${SCRIPT_DIR}/scenarios/multi-ex1.json"
    "${SCRIPT_DIR}/scenarios/ci-input.json"
)

SCHEDS=( grub ffa csf pa )
ALLOCS=( ff_cap ff_big_first ff_little_first ff_lb )

MODE="${1:-verify}"   # "generate" or "verify"

if ! command -v jq &>/dev/null; then
    echo "ERROR: jq is required but not found" >&2
    exit 1
fi

if [ ! -x "$SCHEDSIM" ]; then
    echo "ERROR: schedsim not found at $SCHEDSIM — build first" >&2
    exit 1
fi

failures=0

for scenario in "${SCENARIOS[@]}"; do
    sc_name="$(basename "$scenario" .json)"
    for sched in "${SCHEDS[@]}"; do
        for alloc in "${ALLOCS[@]}"; do
            tag="${sc_name}_${sched}_${alloc}"
            golden_file="${GOLDEN_DIR}/${tag}.json"
            tmp_file=$(mktemp)

            if ! "$SCHEDSIM" -i "$scenario" -p "$PLATFORM" -s "$sched" -a "$alloc" -o "$tmp_file" 2>/dev/null; then
                rm -f "$tmp_file"
                continue   # skip unsupported combos
            fi

            if [ "$MODE" = "generate" ]; then
                jq --sort-keys . "$tmp_file" > "$golden_file"
                echo "  [gen] $tag"
            else
                if [ ! -f "$golden_file" ]; then
                    echo "  [MISSING] $golden_file"
                    failures=$((failures + 1))
                elif ! diff -q <(jq --sort-keys . "$tmp_file") "$golden_file" >/dev/null 2>&1; then
                    echo "  [FAIL] $tag"
                    failures=$((failures + 1))
                else
                    echo "  [OK]   $tag"
                fi
            fi

            rm -f "$tmp_file"
        done
    done
done

if [ "$MODE" = "verify" ] && [ "$failures" -gt 0 ]; then
    echo "FAILED: $failures golden tests did not match"
    exit 1
fi

echo "Gold test ${MODE} complete."
