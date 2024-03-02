#!/bin/sh

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <commit_hash_version1> <commit_hash_version2>"
    exit 1
fi

commit_version1=$1
commit_version2=$2

# Build the first version
git checkout $commit_version1
cmake -S . -B "build_$commit_version1" -G Ninja
cmake --build "build_$commit_version1"

# Build the second version
git checkout $commit_version2
cmake -S . -B "build_$commit_version2" -G Ninja
cmake --build "build_$commit_version2"

# Run the CLI program for both versions and save the output
./"build_$commit_version1"/schedsim/schedsim
./"build_$commit_version2"/schedsim/schedsim

# Clean up (optional)
rm -rf "build_$commit_version1" "build_$commit_version2" output_version1.txt output_version2.txt
