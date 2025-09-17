import pyschedlib as sc  # PYTHONPATH set by CTest

SAMPLE = {
    "tasks": [
        {"id": 1, "utilization": 0.3, "period": 100.0, "jobs": [{"arrival": 0.0, "duration": 10.0}]},
        {"id": 2, "utilization": 0.2, "period": 50.0,  "jobs": [{"arrival": 0.0, "duration": 10.0}]},
    ]
}

import json

s = json.dumps(SAMPLE)

# Old fragile pattern (temporary Setting) would segfault; safe path uses copies
utils = [t.utilization for t in sc.get_tasks_copy(sc.from_json_setting(s))]
assert utils == [0.3, 0.2]

# Also exercise jobs copy helper
tasks = sc.get_tasks_copy(sc.from_json_setting(s))
jobs = sc.get_jobs_copy(tasks[0])
assert len(jobs) == 1 and abs(jobs[0].duration - 10.0) < 1e-9

print('hardening tests passed')

