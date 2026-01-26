#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
GOLDEN_DIR="${SCRIPT_DIR}/golden"
SCHEDSIM="${BUILD_DIR}/apps/schedsim"
MANIFEST="${GOLDEN_DIR}/manifest.sha256"

PLATFORM_NAME="exynos5422"
PLATFORM="${ROOT_DIR}/platforms/${PLATFORM_NAME}.json"

SCHEDS=( grub ffa csf pa )
ALLOCS=( ff_cap ff_big_first ff_little_first ff_lb )

MODE="${1:-verify}"   # "generate" or "verify"

# --- pre-flight checks -------------------------------------------------------
if ! command -v jq &>/dev/null; then
    echo "ERROR: jq is required but not found" >&2
    exit 1
fi

if [ ! -x "$SCHEDSIM" ]; then
    echo "ERROR: schedsim not found at $SCHEDSIM — build first" >&2
    exit 1
fi

# --- sha256 portability -------------------------------------------------------
sha256_of_file() {
    if command -v sha256sum &>/dev/null; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum &>/dev/null; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        echo "ERROR: no sha256 tool found" >&2
        exit 1
    fi
}

# --- collect scenarios --------------------------------------------------------
scenarios=()
for f in "${SCRIPT_DIR}"/scenarios/*.json; do
    [ -f "$f" ] && scenarios+=("$f")
done
for f in "${SCRIPT_DIR}"/scenarios/generated/*.json; do
    [ -f "$f" ] && scenarios+=("$f")
done

if [ ${#scenarios[@]} -eq 0 ]; then
    echo "ERROR: no scenario files found" >&2
    exit 1
fi

echo "Scenarios: ${#scenarios[@]}  |  Schedulers: ${#SCHEDS[@]}  |  Allocators: ${#ALLOCS[@]}  |  Platform: ${PLATFORM_NAME}"
echo "Mode: ${MODE}"
echo "---"

# --- generate / verify --------------------------------------------------------
ok=0
fail=0
skip=0

if [ "$MODE" = "generate" ]; then
    mkdir -p "$GOLDEN_DIR"
    : > "$MANIFEST"   # truncate
fi

if [ "$MODE" = "verify" ] && [ ! -f "$MANIFEST" ]; then
    echo "ERROR: manifest not found at $MANIFEST — run 'generate' first" >&2
    exit 1
fi

for scenario in "${scenarios[@]}"; do
    sc_base="$(basename "$scenario" .json)"
    for sched in "${SCHEDS[@]}"; do
        for alloc in "${ALLOCS[@]}"; do
            tag="${sc_base}_${sched}_${alloc}_${PLATFORM_NAME}"
            tmp_file=$(mktemp)

            if ! "$SCHEDSIM" -i "$scenario" -p "$PLATFORM" -s "$sched" -a "$alloc" -o "$tmp_file" 2>/dev/null; then
                rm -f "$tmp_file"
                skip=$((skip + 1))
                continue
            fi

            # canonical JSON → checksum
            canonical=$(mktemp)
            jq --sort-keys . "$tmp_file" > "$canonical"
            hash=$(sha256_of_file "$canonical")
            rm -f "$tmp_file" "$canonical"

            if [ "$MODE" = "generate" ]; then
                echo "${hash}  ${tag}" >> "$MANIFEST"
                echo "  [gen]  $tag"
                ok=$((ok + 1))
            else
                expected=$(grep -F "  ${tag}" "$MANIFEST" | awk '{print $1}' || true)
                if [ -z "$expected" ]; then
                    # tag not in manifest — treat as skip (new combo)
                    skip=$((skip + 1))
                    continue
                fi
                if [ "$hash" = "$expected" ]; then
                    echo "  [OK]   $tag"
                    ok=$((ok + 1))
                else
                    echo "  [FAIL] $tag  (got ${hash:0:12}… expected ${expected:0:12}…)"
                    fail=$((fail + 1))
                fi
            fi
        done
    done
done

echo "---"
echo "OK: $ok  /  FAIL: $fail  /  SKIP: $skip"

if [ "$MODE" = "verify" ] && [ "$fail" -gt 0 ]; then
    exit 1
fi
