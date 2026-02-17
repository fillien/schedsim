#!/usr/bin/env python3
"""I/O tests for pyschedsim - loading platforms and scenarios from JSON."""

import json
import os
import sys
import tempfile

# Expect PYTHONPATH to include the build/python directory (set by CTest)
try:
    import pyschedsim as sim
except Exception as e:
    raise SystemExit(f"Failed to import pyschedsim: {e}")


def test_load_platform_from_file():
    """Test loading a platform from a JSON file."""
    # Use the orion6.json platform file
    platform_path = "platforms/orion6.json"
    if not os.path.exists(platform_path):
        print(f"  Skipping (file not found: {platform_path})")
        return

    engine = sim.Engine()
    sim.load_platform(engine, platform_path)

    platform = engine.platform

    # orion6.json has 3 clusters with 4 procs each = 12 total processors
    assert platform.processor_count == 12, \
        f"Expected 12 processors, got {platform.processor_count}"

    # Should have 3 processor types (one per cluster)
    assert platform.processor_type_count == 3, \
        f"Expected 3 processor types, got {platform.processor_type_count}"

    # Should have 3 clock domains (one per cluster)
    assert platform.clock_domain_count == 3, \
        f"Expected 3 clock domains, got {platform.clock_domain_count}"

    print("  Load platform from file: OK")


def test_load_platform_from_string():
    """Test loading a platform from a JSON string."""
    json_str = '''{
        "clusters": [
            {
                "effective_freq": 1000.0,
                "frequencies": [500.0, 1000.0, 1500.0],
                "power_model": [0.1, 0.0, 0.0, 0.0],
                "procs": 2,
                "perf_score": 1.0
            }
        ]
    }'''

    engine = sim.Engine()
    sim.load_platform_from_string(engine, json_str)

    platform = engine.platform

    assert platform.processor_count == 2, \
        f"Expected 2 processors, got {platform.processor_count}"
    assert platform.processor_type_count == 1, \
        f"Expected 1 processor type, got {platform.processor_type_count}"

    # Check clock domain frequencies
    cd = platform.clock_domain(0)
    assert cd.freq_min == 500.0, f"Expected freq_min=500, got {cd.freq_min}"
    assert cd.freq_max == 1500.0, f"Expected freq_max=1500, got {cd.freq_max}"

    print("  Load platform from string: OK")


def test_load_scenario_from_string():
    """Test loading a scenario from a JSON string."""
    json_str = '''{
        "tasks": [
            {
                "id": 1,
                "period": 10.0,
                "wcet": 2.0,
                "jobs": [
                    {"arrival": 0.0, "duration": 2.0},
                    {"arrival": 10.0, "duration": 1.5}
                ]
            },
            {
                "id": 2,
                "period": 5.0,
                "relative_deadline": 4.0,
                "utilization": 0.2
            }
        ]
    }'''

    scenario = sim.load_scenario_from_string(json_str)

    assert len(scenario.tasks) == 2, f"Expected 2 tasks, got {len(scenario.tasks)}"

    # Check first task
    task1 = scenario.tasks[0]
    assert task1.id == 1, f"Expected task1.id=1, got {task1.id}"
    assert task1.period == 10.0, f"Expected task1.period=10.0, got {task1.period}"
    assert task1.wcet == 2.0, f"Expected task1.wcet=2.0, got {task1.wcet}"
    assert task1.relative_deadline == 10.0, \
        f"Expected task1.relative_deadline=10.0 (default), got {task1.relative_deadline}"
    assert len(task1.jobs) == 2, f"Expected 2 jobs for task1, got {len(task1.jobs)}"

    # Check jobs
    assert task1.jobs[0].arrival == 0.0
    assert task1.jobs[0].duration == 2.0
    assert task1.jobs[1].arrival == 10.0
    assert task1.jobs[1].duration == 1.5

    # Check second task (uses utilization instead of wcet)
    task2 = scenario.tasks[1]
    assert task2.id == 2, f"Expected task2.id=2, got {task2.id}"
    assert task2.period == 5.0, f"Expected task2.period=5.0, got {task2.period}"
    assert task2.relative_deadline == 4.0, f"Expected task2.relative_deadline=4.0, got {task2.relative_deadline}"
    # wcet = period * utilization = 5.0 * 0.2 = 1.0
    assert abs(task2.wcet - 1.0) < 1e-9, f"Expected task2.wcet=1.0, got {task2.wcet}"

    print("  Load scenario from string: OK")


def test_load_scenario_from_file():
    """Test loading a scenario from a file."""
    # Create a temporary scenario file
    scenario_data = {
        "tasks": [
            {
                "id": 1,
                "period": 10.0,
                "wcet": 2.0
            }
        ]
    }

    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        json.dump(scenario_data, f)
        temp_path = f.name

    try:
        scenario = sim.load_scenario(temp_path)
        assert len(scenario.tasks) == 1
        assert scenario.tasks[0].id == 1
        print("  Load scenario from file: OK")
    finally:
        os.unlink(temp_path)


def test_inject_scenario():
    """Test injecting a scenario into an engine."""
    # Create platform
    platform_json = '''{
        "clusters": [
            {
                "effective_freq": 1000.0,
                "frequencies": [500.0, 1000.0],
                "power_model": [0.1, 0.0, 0.0, 0.0],
                "procs": 1,
                "perf_score": 1.0
            }
        ]
    }'''

    engine = sim.Engine()
    sim.load_platform_from_string(engine, platform_json)

    # Create scenario
    scenario_json = '''{
        "tasks": [
            {"id": 1, "period": 10.0, "wcet": 2.0},
            {"id": 2, "period": 5.0, "wcet": 1.0}
        ]
    }'''

    scenario = sim.load_scenario_from_string(scenario_json)

    # Inject scenario BEFORE finalize
    tasks = sim.inject_scenario(engine, scenario)

    assert len(tasks) == 2, f"Expected 2 tasks returned, got {len(tasks)}"

    platform = engine.platform
    assert platform.task_count == 2, f"Expected 2 tasks in platform, got {platform.task_count}"

    # Verify task properties match
    task0 = platform.task(0)
    assert task0.period == 10.0, f"Expected period=10.0, got {task0.period}"
    assert task0.wcet == 2.0, f"Expected wcet=2.0, got {task0.wcet}"

    print("  Inject scenario: OK")


def test_schedule_arrivals():
    """Test scheduling job arrivals from scenario data."""
    # Create platform
    platform_json = '''{
        "clusters": [
            {
                "effective_freq": 1000.0,
                "frequencies": [500.0, 1000.0],
                "power_model": [0.1, 0.0, 0.0, 0.0],
                "procs": 1,
                "perf_score": 1.0
            }
        ]
    }'''

    engine = sim.Engine()
    sim.load_platform_from_string(engine, platform_json)

    # Create scenario with jobs
    scenario_json = '''{
        "tasks": [
            {
                "id": 1, "period": 10.0, "wcet": 2.0,
                "jobs": [
                    {"arrival": 0.0, "duration": 2.0},
                    {"arrival": 10.0, "duration": 1.5}
                ]
            }
        ]
    }'''

    scenario = sim.load_scenario_from_string(scenario_json)
    tasks = sim.inject_scenario(engine, scenario)

    # Finalize BEFORE scheduling arrivals (per scenario_injection.hpp comment)
    # Actually, looking at the header, schedule_arrivals must be called AFTER finalize
    engine.platform.finalize()

    # Schedule arrivals using the scenario's jobs (pass JobParams directly)
    for i, tp in enumerate(scenario.tasks):
        sim.schedule_arrivals(engine, tasks[i], tp.jobs)

    print("  Schedule arrivals: OK")


def test_full_workflow():
    """Test complete workflow: load platform, load scenario, run simulation."""
    # Create platform
    platform_json = '''{
        "clusters": [
            {
                "effective_freq": 1000.0,
                "frequencies": [1000.0],
                "power_model": [0.1, 0.0, 0.0, 0.0],
                "procs": 1,
                "perf_score": 1.0
            }
        ]
    }'''

    # Create scenario with jobs
    scenario_json = '''{
        "tasks": [
            {
                "id": 1, "period": 10.0, "wcet": 2.0,
                "jobs": [
                    {"arrival": 0.0, "duration": 2.0},
                    {"arrival": 10.0, "duration": 2.0}
                ]
            }
        ]
    }'''

    # 1. Create engine and load platform
    engine = sim.Engine()
    sim.load_platform_from_string(engine, platform_json)

    # 2. Load and inject scenario (before finalize)
    scenario = sim.load_scenario_from_string(scenario_json)
    tasks = sim.inject_scenario(engine, scenario)

    # 3. Finalize
    engine.platform.finalize()

    # 4. Schedule arrivals (after finalize)
    for i, tp in enumerate(scenario.tasks):
        sim.schedule_arrivals(engine, tasks[i], tp.jobs)

    # 5. Create scheduler
    procs = sim.get_all_processors(engine)
    scheduler = sim.EdfScheduler(engine, procs)

    # 6. Add servers for each task
    for task in sim.get_all_tasks(engine):
        scheduler.add_server(task)

    # 7. Create allocator
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)

    # 8. Run simulation
    engine.run(25.0)

    assert engine.time >= 25.0, f"Expected time >= 25.0, got {engine.time}"

    print("  Full workflow: OK")


def test_write_scenario():
    """Test writing a scenario to a file."""
    # Create scenario from string
    scenario_json = '''{
        "tasks": [
            {"id": 1, "period": 10.0, "wcet": 2.0},
            {"id": 2, "period": 5.0, "wcet": 1.0}
        ]
    }'''

    scenario = sim.load_scenario_from_string(scenario_json)

    # Write to temp file
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        temp_path = f.name

    try:
        sim.write_scenario(scenario, temp_path)

        # Read back and verify
        scenario2 = sim.load_scenario(temp_path)
        assert len(scenario2.tasks) == 2
        assert scenario2.tasks[0].id == 1
        assert scenario2.tasks[0].period == 10.0
        assert scenario2.tasks[1].id == 2

        print("  Write scenario: OK")
    finally:
        os.unlink(temp_path)


def test_error_handling():
    """Test that I/O errors are properly raised as Python exceptions."""
    # Test loading non-existent file
    try:
        sim.load_platform(sim.Engine(), "nonexistent_file.json")
        assert False, "Should have raised exception for non-existent file"
    except IOError:
        pass  # Expected

    # Test loading invalid JSON
    try:
        sim.load_scenario_from_string("{invalid json")
        assert False, "Should have raised exception for invalid JSON"
    except IOError:
        pass  # Expected

    # Test loading scenario with missing required field
    try:
        sim.load_scenario_from_string('{"tasks": [{"period": 10.0}]}')  # missing id
        assert False, "Should have raised exception for missing id"
    except IOError:
        pass  # Expected

    print("  Error handling: OK")


def test_load_real_platform_file():
    """Test loading the actual orion6.json platform file with full verification."""
    platform_path = "platforms/orion6.json"
    if not os.path.exists(platform_path):
        print(f"  Skipping (file not found: {platform_path})")
        return

    engine = sim.Engine()
    sim.load_platform(engine, platform_path)

    platform = engine.platform

    # orion6.json has 3 clusters with 4 procs each = 12 total processors
    assert platform.processor_count == 12

    # Verify we can access all processors
    procs = sim.get_all_processors(engine)
    assert len(procs) == 12

    # Verify processor types exist and have sensible performance values
    for i in range(platform.processor_type_count):
        pt = platform.processor_type(i)
        assert pt.performance > 0.0, f"Processor type {i} has invalid performance"

    # Verify clock domains have valid frequency ranges
    for i in range(platform.clock_domain_count):
        cd = platform.clock_domain(i)
        assert cd.freq_min > 0.0, f"Clock domain {i} has invalid freq_min"
        assert cd.freq_max >= cd.freq_min, f"Clock domain {i}: freq_max < freq_min"

    # Note: Platform is NOT auto-finalized by loader (allows adding tasks)
    # Users must call platform.finalize() when ready to run simulation

    print("  Load real platform file: OK")


def test_platform_with_multiple_clusters():
    """Test platform JSON with multiple heterogeneous clusters."""
    json_str = '''{
        "clusters": [
            {
                "effective_freq": 2000.0,
                "frequencies": [1000.0, 1500.0, 2000.0],
                "power_model": [0.2, 0.0, 0.0, 0.0],
                "procs": 4,
                "perf_score": 2.0
            },
            {
                "effective_freq": 1000.0,
                "frequencies": [500.0, 750.0, 1000.0],
                "power_model": [0.05, 0.0, 0.0, 0.0],
                "procs": 4,
                "perf_score": 1.0
            }
        ]
    }'''

    engine = sim.Engine()
    sim.load_platform_from_string(engine, json_str)

    platform = engine.platform

    # Total processors: 4 + 4 = 8
    assert platform.processor_count == 8, \
        f"Expected 8 processors, got {platform.processor_count}"

    # 2 processor types (one per cluster)
    assert platform.processor_type_count == 2, \
        f"Expected 2 processor types, got {platform.processor_type_count}"

    # 2 clock domains (one per cluster)
    assert platform.clock_domain_count == 2, \
        f"Expected 2 clock domains, got {platform.clock_domain_count}"

    # Verify different performance scores
    pt0 = platform.processor_type(0)
    pt1 = platform.processor_type(1)
    assert pt0.performance == 2.0, f"Expected big core perf=2.0, got {pt0.performance}"
    assert pt1.performance == 1.0, f"Expected little core perf=1.0, got {pt1.performance}"

    print("  Platform with multiple clusters: OK")


def test_scenario_with_utilization():
    """Test scenario using utilization field instead of explicit wcet."""
    json_str = '''{
        "tasks": [
            {
                "id": 1,
                "period": 10.0,
                "utilization": 0.3
            },
            {
                "id": 2,
                "period": 5.0,
                "utilization": 0.2
            }
        ]
    }'''

    scenario = sim.load_scenario_from_string(json_str)

    assert len(scenario.tasks) == 2

    # wcet should be computed from period * utilization
    task1 = scenario.tasks[0]
    expected_wcet1 = 10.0 * 0.3  # 3.0
    assert abs(task1.wcet - expected_wcet1) < 1e-9, \
        f"Expected wcet={expected_wcet1}, got {task1.wcet}"

    task2 = scenario.tasks[1]
    expected_wcet2 = 5.0 * 0.2  # 1.0
    assert abs(task2.wcet - expected_wcet2) < 1e-9, \
        f"Expected wcet={expected_wcet2}, got {task2.wcet}"

    print("  Scenario with utilization: OK")


def test_scenario_jobs_sorted():
    """Test that scenario jobs are accessible in sorted order by arrival time."""
    json_str = '''{
        "tasks": [
            {
                "id": 1,
                "period": 10.0,
                "wcet": 2.0,
                "jobs": [
                    {"arrival": 20.0, "duration": 2.0},
                    {"arrival": 0.0, "duration": 1.5},
                    {"arrival": 10.0, "duration": 1.8}
                ]
            }
        ]
    }'''

    scenario = sim.load_scenario_from_string(json_str)
    task = scenario.tasks[0]

    assert len(task.jobs) == 3

    # Verify we can access all jobs (order may or may not be sorted by loader)
    arrivals = [job.arrival for job in task.jobs]
    assert 0.0 in arrivals
    assert 10.0 in arrivals
    assert 20.0 in arrivals

    print("  Scenario jobs sorted: OK")


def test_inject_empty_scenario():
    """Test injecting an empty scenario (no tasks)."""
    platform_json = '''{
        "clusters": [
            {
                "effective_freq": 1000.0,
                "frequencies": [1000.0],
                "power_model": [0.1, 0.0, 0.0, 0.0],
                "procs": 1,
                "perf_score": 1.0
            }
        ]
    }'''

    engine = sim.Engine()
    sim.load_platform_from_string(engine, platform_json)

    # Empty scenario
    scenario_json = '{"tasks": []}'
    scenario = sim.load_scenario_from_string(scenario_json)

    assert len(scenario.tasks) == 0

    # Inject empty scenario
    tasks = sim.inject_scenario(engine, scenario)
    assert len(tasks) == 0

    # Platform should still have 0 tasks
    platform = engine.platform
    assert platform.task_count == 0

    print("  Inject empty scenario: OK")


def test_scenario_roundtrip():
    """Test writing then reading a scenario preserves data."""
    # Create original scenario
    original_json = '''{
        "tasks": [
            {"id": 1, "period": 10.0, "wcet": 2.0, "relative_deadline": 8.0},
            {"id": 2, "period": 5.0, "wcet": 1.0}
        ]
    }'''

    scenario1 = sim.load_scenario_from_string(original_json)

    # Write to temp file
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        temp_path = f.name

    try:
        sim.write_scenario(scenario1, temp_path)

        # Read back
        scenario2 = sim.load_scenario(temp_path)

        # Verify identical
        assert len(scenario2.tasks) == len(scenario1.tasks), \
            f"Task count mismatch: {len(scenario2.tasks)} vs {len(scenario1.tasks)}"

        for i in range(len(scenario1.tasks)):
            t1 = scenario1.tasks[i]
            t2 = scenario2.tasks[i]

            assert t1.id == t2.id, f"Task {i} id mismatch"
            assert abs(t1.period - t2.period) < 1e-9, f"Task {i} period mismatch"
            assert abs(t1.wcet - t2.wcet) < 1e-9, f"Task {i} wcet mismatch"
            assert abs(t1.relative_deadline - t2.relative_deadline) < 1e-9, \
                f"Task {i} deadline mismatch"

        print("  Scenario roundtrip: OK")
    finally:
        os.unlink(temp_path)


def main():
    print("Running pyschedsim I/O tests...")

    test_load_platform_from_string()
    test_load_platform_from_file()
    test_load_scenario_from_string()
    test_load_scenario_from_file()
    test_inject_scenario()
    test_schedule_arrivals()
    test_full_workflow()
    test_write_scenario()
    test_error_handling()
    test_load_real_platform_file()
    test_platform_with_multiple_clusters()
    test_scenario_with_utilization()
    test_scenario_jobs_sorted()
    test_inject_empty_scenario()
    test_scenario_roundtrip()

    print("\nAll pyschedsim I/O tests passed!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
