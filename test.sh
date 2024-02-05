#!/bin/sh

./build/schedsim/schedsim -s sce_test.json --policy "grub" -o logs-grub.json
./build/schedview/schedview logs-grub.json --html > gantt-grub.html
./build/schedsim/schedsim -s sce_test.json --policy "pa" -o logs-pa.json
./build/schedview/schedview logs-pa.json --html > gantt-pa.html
