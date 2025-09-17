import os
import sys
from pathlib import Path

# PYTHONPATH set by CTest to include build/python
import pyschedlib as sc  # type: ignore

# 1) generate_tasksets to a non-existent directory -> ValueError (std::invalid_argument)
try:
    sc.generate_tasksets(
        output_path='__no_such_dir__',
        nb_taskset=1,
        nb_tasks=1,
        total_utilization=0.5,
        umax=0.5,
        umin=0.1,
        success_rate=1.0,
        compression_rate=1.0,
    )
    raise SystemExit('Expected ValueError was not raised')
except ValueError as e:
    pass

# 2) from_json_setting with invalid JSON -> RuntimeError
try:
    sc.from_json_setting('{ invalid json')
    raise SystemExit('Expected RuntimeError was not raised')
except RuntimeError:
    pass

print('exception mapping tests passed')

