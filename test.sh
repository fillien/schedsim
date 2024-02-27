#!/bin/sh +x

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <commit_hash_version1> <commit_hash_version2>"
    exit 1
fi

VERSION1=$1
VERSION2=$2

TESTS_DIR=./build_sce
PLATFORM_FILE=$TESTS_DIR/platform.json
SCENARIO_DIR=$TESTS_DIR/scenarios
LOGS_DIR=$TESTS_DIR/logs
mkdir "$TESTS_DIR"

# Build the first version
git checkout "$VERSION1"
cmake -S . -B "build_$VERSION1" -G Ninja
cmake --build "build_$VERSION1" 

# Build the second version
git checkout "$VERSION2"
cmake -S . -B "build_$VERSION2" -G Ninja
cmake --build "build_$VERSION2" 

# Generate platform and scenarios
./"build_$VERSION1"/schedgen/schedgen platform --output "$PLATFORM_FILE" --cores 2 --freq 1
mkdir "${SCENARIO_DIR}"
for i in {1..100}; do
    NB_TASKS=5
    NB_JOBS=5
    TOTALU=1.5
    ./"build_$VERSION1"/schedgen/schedgen taskset -o "${SCENARIO_DIR}/${i}.json" --tasks "$NB_TASKS" --jobs "$NB_JOBS" --totalu "$TOTALU" --success 1
done

ls -l "$TESTS_DIR"

# Run the CLI program for both versions and compare the outputs
mkdir -p "${LOGS_DIR}/${VERSION1}" "${LOGS_DIR}/${VERSION2}"
for file in "$SCENARIO_DIR"/*.json
do
    echo $file
    ./"build_$VERSION1"/schedsim/schedsim --platform "$PLATFORM_FILE" --scenario "$file" --policy "grub" --output "$LOGS_DIR/$VERSION1/$(basename "$file")"
    ./"build_$VERSION2"/schedsim/schedsim --platform "$PLATFORM_FILE" --scenario "$file" --policy "grub" --output "$LOGS_DIR/$VERSION2/$(basename "$file")"

    # Compare the outputs logs
    diff <(jq --sort-keys . "$LOGS_DIR/$VERSION1/$(basename "$file")") <(jq --sort-keys . "$LOGS_DIR/$VERSION2/$(basename "$file")")
done

# Clean up (optional)
#rm -rf "build_$VERSION1" "build_$VERSION2" output_version1.txt output_version2.txt

git checkout "$VERSION1"
