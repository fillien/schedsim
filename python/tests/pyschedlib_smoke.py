import os
import sys

# Expect PYTHONPATH to include the build/python directory (set by CTest)
try:
    import pyschedlib  # type: ignore
except Exception as e:
    raise SystemExit(f"Failed to import pyschedlib: {e}")

assert hasattr(pyschedlib, "uunifast_discard_weibull"), "uunifast_discard_weibull not exported"
assert hasattr(pyschedlib, "from_utilizations"), "from_utilizations not exported"
assert hasattr(pyschedlib, "write_setting_file") and hasattr(pyschedlib, "read_setting_file"), "file IO wrappers not exported"

# Generate two small task sets; also exercise optional pair typemap
s1 = pyschedlib.uunifast_discard_weibull(3, 0.9, 0.7, 0.05, 1.0, 1.0, None)
assert hasattr(s1, "tasks") and len(s1.tasks) > 0, "Empty task set from generator"

s2 = pyschedlib.uunifast_discard_weibull(2, 0.5, 0.6, 0.05, 1.0, 1.0, (0.1, 0.2))
assert len(s2.tasks) > 0, "Second generated task set is empty"

# Combine and check sizes
s3 = pyschedlib.add_taskets(s1, s2)
assert len(s3.tasks) == len(s1.tasks) + len(s2.tasks), "add_taskets size mismatch"

# Build a setting from a custom utilization vector
utils = [0.3, 0.2, 0.1]
s4 = pyschedlib.from_utilizations(utils, success_rate=1.0, compression_rate=1.0)
assert len(s4.tasks) == len(utils), "from_utilizations wrong number of tasks"
got_utils = [round(t.utilization, 6) for t in s4.tasks]
exp_utils = [round(u, 6) for u in utils]
assert got_utils == exp_utils, f"from_utilizations utilizations mismatch: {got_utils} vs {exp_utils}"

# Verify round-trip file IO
_tmp = "tmp_test_setting.json"
pyschedlib.write_setting_file(_tmp, s4)
s4b = pyschedlib.read_setting_file(_tmp)
assert len(s4b.tasks) == len(utils), "read_setting_file wrong number of tasks"

print("pyschedlib smoke test passed")
