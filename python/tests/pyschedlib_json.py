import os
import sys

# PYTHONPATH is set by CTest to include build/python
import pyschedlib  # type: ignore

sample = {
    "tasks": [
        {
            "id": 1,
            "utilization": 0.3,
            "period": 100.0,
            "jobs": [
                {"arrival": 0.0, "duration": 10.0},
                {"arrival": 100.0, "duration": 10.0},
            ],
        },
        {
            "id": 2,
            "utilization": 0.2,
            "period": 50.0,
            "jobs": [
                {"arrival": 0.0, "duration": 10.0},
            ],
        },
    ]
}

import json

s = json.dumps(sample)
setting = pyschedlib.from_json_setting(s)

assert hasattr(setting, "tasks"), "Setting has no tasks attribute"
assert len(setting.tasks) == 2, f"Expected 2 tasks, got {len(setting.tasks)}"
assert setting.tasks[0].id == 1
assert abs(setting.tasks[0].period - 100.0) < 1e-9
assert len(setting.tasks[0].jobs) == 2

print("pyschedlib JSON test passed")

