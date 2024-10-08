#!/usr/bin/env bash

set -o errexit
set -o nounset
set -o pipefail

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <commit_hash_version1> <commit_hash_version2>"
    exit 1
fi

COMMIT_BACKUP=$(git rev-parse HEAD)

# Call exit with the exit code in arg
function finish() {
    git checkout "$COMMIT_BACKUP"
    exit "$1"
}

# Test if current branch is the one speficied in arg 1
function ami_commit() {
    if [ "$1" != "$(git rev-parse HEAD)" ]; then
	echo "The script is on the wrong commit"
	finish 1	
    fi
}

VERSION1=$(git rev-parse "$1")
VERSION2=$(git rev-parse "$2")
NB_TASKS=20

TESTS_DIR=./build_sce
PLATFORM_FILE=$TESTS_DIR/platform.json
SCENARIO_DIR=$TESTS_DIR/scenarios
LOGS_DIR=$TESTS_DIR/logs

if [[ -e "${TESTS_DIR}" ]]; then
    rm -rf ${TESTS_DIR}
fi
mkdir "${TESTS_DIR}"

# Build the first version
git checkout "${VERSION1}"
ami_commit "${VERSION1}"
cmake -S . -B "build_${VERSION1}" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "build_${VERSION1}" 

# Build the second version
git checkout "${VERSION2}"
ami_commit "${VERSION2}"
cmake -S . -B "build_${VERSION2}" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "build_${VERSION2}" 

# Generate platform and scenarios
./"build_${VERSION1}"/schedgen/schedgen platform --output "${PLATFORM_FILE}" --cores 2 --freq 1,2,3,4,5,6,7,8,9,10 --eff 3
mkdir -p "${SCENARIO_DIR}"
for i in {1..20}; do
    NB_JOBS=80
    TOTALU=1.5
    ./"build_${VERSION1}"/schedgen/schedgen taskset -o "${SCENARIO_DIR}/${i}.json" --tasks "${NB_TASKS}" --jobs "${NB_JOBS}" --totalu "${TOTALU}" --success 1
done
echo "Generate 20 tasksets, with ${NB_TASKS} tasks, ${NB_JOBS} jobs and a total utilization of ${TOTALU}"

# Run the CLI program for both versions and compare the outputs
mkdir -p "${LOGS_DIR}/${VERSION1}" "${LOGS_DIR}/${VERSION2}"
for file in "${SCENARIO_DIR}"/*.json
do
    echo "${file}"
    ./"build_${VERSION1}"/schedsim/schedsim --platform "${PLATFORM_FILE}" --scenario "${file}" --policy "grub" --output "${LOGS_DIR}/${VERSION1}/$(basename "$file")"
    ./"build_${VERSION2}"/schedsim/schedsim --platform "${PLATFORM_FILE}" --scenario "${file}" --policy "grub" --output "${LOGS_DIR}/$VERSION2/$(basename "$file")"
    diff <(jq --sort-keys . "${LOGS_DIR}/${VERSION1}/$(basename "$file")") <(jq --sort-keys . "${LOGS_DIR}/${VERSION2}/$(basename "$file")")

    ./"build_${VERSION1}"/schedsim/schedsim --platform "${PLATFORM_FILE}" --scenario "${file}" --policy "pa" --output "${LOGS_DIR}/${VERSION1}/$(basename "$file")"
    ./"build_${VERSION2}"/schedsim/schedsim --platform "${PLATFORM_FILE}" --scenario "${file}" --policy "pa" --output "${LOGS_DIR}/${VERSION2}/$(basename "$file")"
    diff <(jq --sort-keys . "${LOGS_DIR}/${VERSION1}/$(basename "$file")") <(jq --sort-keys . "${LOGS_DIR}/${VERSION2}/$(basename "$file")")

    ./"build_${VERSION1}"/schedsim/schedsim --platform "${PLATFORM_FILE}" --scenario "${file}" --policy "pa_f_min" --output "${LOGS_DIR}/${VERSION1}/$(basename "$file")"
    ./"build_${VERSION2}"/schedsim/schedsim --platform "${PLATFORM_FILE}" --scenario "${file}" --policy "pa_f_min" --output "${LOGS_DIR}/${VERSION2}/$(basename "$file")"
    diff <(jq --sort-keys . "${LOGS_DIR}/${VERSION1}/$(basename "$file")") <(jq --sort-keys . "${LOGS_DIR}/${VERSION2}/$(basename "$file")")

    ./"build_${VERSION1}"/schedsim/schedsim --platform "${PLATFORM_FILE}" --scenario "${file}" --policy "pa_m_min" --output "${LOGS_DIR}/${VERSION1}/$(basename "$file")"
    ./"build_${VERSION2}"/schedsim/schedsim --platform "${PLATFORM_FILE}" --scenario "${file}" --policy "pa_m_min" --output "${LOGS_DIR}/${VERSION2}/$(basename "$file")"
    diff <(jq --sort-keys . "${LOGS_DIR}/${VERSION1}/$(basename "$file")") <(jq --sort-keys . "${LOGS_DIR}/${VERSION2}/$(basename "$file")")
done

finish 0
