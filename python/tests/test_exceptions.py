#!/usr/bin/env python3
"""Exception handling tests for pyschedsim - validates error mapping to Python exceptions."""

import json
import os
import sys
import tempfile

try:
    import pyschedsim as sim
except Exception as e:
    raise SystemExit(f"Failed to import pyschedsim: {e}")


def test_already_finalized_error():
    """Test that AlreadyFinalizedError is raised when modifying after finalize."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Build minimal platform
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    platform.add_processor(pt, cd, pd)

    # Finalize
    platform.finalize()
    assert platform.is_finalized()

    # Try to add after finalize - should raise RuntimeError
    try:
        platform.add_processor_type("cpu2", 0.5)
        assert False, "Should have raised exception for add after finalize"
    except RuntimeError as e:
        # Expected - AlreadyFinalizedError maps to RuntimeError
        assert "finalized" in str(e).lower() or "cannot" in str(e).lower(), \
            f"Unexpected error message: {e}"

    print("  AlreadyFinalizedError: OK")


def test_admission_error():
    """Test that AdmissionError is raised for over-utilization."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Build platform with 1 processor
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    proc = platform.add_processor(pt, cd, pd)

    # Add tasks that will exceed capacity
    task1 = platform.add_task(10.0, 10.0, 8.0)  # utilization = 0.8
    task2 = platform.add_task(10.0, 10.0, 5.0)  # utilization = 0.5
    platform.finalize()

    # Create scheduler
    scheduler = sim.EdfScheduler(engine, [proc])

    # First server should be admitted
    scheduler.add_server(task1)

    # Second server should NOT be admitted (0.8 + 0.5 > 1.0)
    try:
        scheduler.add_server(task2)
        assert False, "Should have raised exception for over-utilization"
    except ValueError as e:
        # Expected - AdmissionError maps to ValueError
        pass

    print("  AdmissionError: OK")


def test_loader_error_file_not_found():
    """Test that LoaderError is raised for non-existent files."""
    engine = sim.Engine()

    try:
        sim.load_platform(engine, "/nonexistent/path/to/file.json")
        assert False, "Should have raised exception for non-existent file"
    except IOError as e:
        # Expected - LoaderError maps to IOError
        pass

    # Same for scenario loading
    try:
        sim.load_scenario("/nonexistent/path/to/scenario.json")
        assert False, "Should have raised exception for non-existent scenario file"
    except IOError as e:
        # Expected
        pass

    print("  LoaderError (file not found): OK")


def test_loader_error_invalid_json():
    """Test that LoaderError is raised for invalid JSON."""
    engine = sim.Engine()

    # Invalid JSON syntax
    try:
        sim.load_platform_from_string(engine, "{invalid json")
        assert False, "Should have raised exception for invalid JSON"
    except IOError as e:
        # Expected - parse errors map to IOError
        pass

    # Also test with scenario
    try:
        sim.load_scenario_from_string("{not valid: json,}")
        assert False, "Should have raised exception for invalid JSON"
    except IOError as e:
        # Expected
        pass

    print("  LoaderError (invalid JSON): OK")


def test_loader_error_missing_field():
    """Test that LoaderError is raised for missing required fields."""
    # Missing 'id' field in task
    try:
        sim.load_scenario_from_string('{"tasks": [{"period": 10.0, "wcet": 2.0}]}')
        assert False, "Should have raised exception for missing id"
    except IOError as e:
        # Expected
        pass

    # Missing 'period' field
    try:
        sim.load_scenario_from_string('{"tasks": [{"id": 1, "wcet": 2.0}]}')
        assert False, "Should have raised exception for missing period"
    except IOError as e:
        # Expected
        pass

    print("  LoaderError (missing field): OK")


def test_invalid_cstate_tuple():
    """Test that invalid C-state tuples raise ValueError."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Wrong tuple size (3 elements instead of 4)
    try:
        platform.add_power_domain([(0, 0, 0.0)])  # Missing power
        assert False, "Should have raised exception for wrong tuple size"
    except (ValueError, RuntimeError):
        # Expected
        pass

    # Not a list
    try:
        platform.add_power_domain("not a list")
        assert False, "Should have raised exception for non-list"
    except (ValueError, RuntimeError, TypeError):
        # Expected
        pass

    print("  Invalid C-state tuple: OK")


def main():
    print("Running pyschedsim exception handling tests...")

    test_already_finalized_error()
    test_admission_error()
    test_loader_error_file_not_found()
    test_loader_error_invalid_json()
    test_loader_error_missing_field()
    test_invalid_cstate_tuple()

    print("\nAll pyschedsim exception tests passed!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
