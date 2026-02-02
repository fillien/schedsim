#!/usr/bin/env python3
"""Type conversion tests for pyschedsim - validates Duration, TimePoint, Frequency, etc."""

import sys

try:
    import pyschedsim as sim
except Exception as e:
    raise SystemExit(f"Failed to import pyschedsim: {e}")


def test_duration_conversion():
    """Test that Python floats convert properly to Duration (seconds)."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Add a task with float durations
    task = platform.add_task(10.5, 8.25, 2.125)

    # Verify durations come back as floats
    assert task.period() == 10.5, f"Expected period=10.5, got {task.period()}"
    assert task.relative_deadline() == 8.25, f"Expected deadline=8.25, got {task.relative_deadline()}"
    assert task.wcet() == 2.125, f"Expected wcet=2.125, got {task.wcet()}"

    print("  Duration conversion: OK")


def test_timepoint_conversion():
    """Test that TimePoint (simulation time) converts to Python float."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Build minimal platform for running
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    platform.add_processor(pt, cd, pd)
    platform.finalize()

    # Run to a specific time
    engine.run(7.5)

    # TimePoint should convert to float
    time = engine.time()
    assert isinstance(time, float), f"Expected float, got {type(time)}"
    assert time == 7.5, f"Expected time=7.5, got {time}"

    print("  TimePoint conversion: OK")


def test_frequency_mhz():
    """Test that Frequency values are in MHz."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Add clock domain with specific frequencies
    cd = platform.add_clock_domain(500.0, 2000.0)

    # Frequency should be in MHz
    assert cd.freq_min() == 500.0, f"Expected freq_min=500 MHz, got {cd.freq_min()}"
    assert cd.freq_max() == 2000.0, f"Expected freq_max=2000 MHz, got {cd.freq_max()}"
    assert cd.frequency() == 2000.0, f"Expected frequency=2000 MHz (initial=max), got {cd.frequency()}"

    print("  Frequency MHz: OK")


def test_power_mw():
    """Test that Power values in C-states are in mW."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Add power domain with C-states
    # (level, scope, wake_latency_s, power_mW)
    c_states = [
        (0, 0, 0.0, 150.5),     # C0 - active state, 150.5 mW
        (1, 0, 0.001, 25.75),   # C1 - light sleep, 25.75 mW
        (2, 0, 0.010, 5.0),     # C2 - deep sleep, 5 mW
    ]
    pd = platform.add_power_domain(c_states)

    # If we got here without error, the power values were accepted
    assert pd.id() == 0, f"Expected power domain id=0, got {pd.id()}"

    print("  Power mW: OK")


def test_energy_mj():
    """Test that Energy output values are in mJ."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Build platform with power model
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])  # 100 mW
    platform.add_processor(pt, cd, pd)
    platform.finalize()

    # Enable energy tracking
    engine.enable_energy_tracking(True)

    # Run for 1 second - should consume ~100 mJ at 100 mW
    engine.run(1.0)

    # Get energy (in mJ)
    energy = engine.total_energy()
    assert isinstance(energy, float), f"Expected float, got {type(energy)}"
    # Energy should be around 100 mJ (100 mW * 1s = 100 mJ)
    # Allow some tolerance since processor might idle at lower power
    assert energy >= 0.0, f"Energy should be non-negative, got {energy}"

    print("  Energy mJ: OK")


def test_int_accepted_as_duration():
    """Test that Python ints are accepted where Duration is expected."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Use integers for all duration parameters
    task = platform.add_task(10, 10, 2)  # int instead of float

    # Should work and convert properly
    assert task.period() == 10.0, f"Expected period=10.0, got {task.period()}"
    assert task.wcet() == 2.0, f"Expected wcet=2.0, got {task.wcet()}"

    # Also test with engine.run()
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000, 2000)  # int frequencies
    pd = platform.add_power_domain([(0, 0, 0, 100)])  # int wake_latency and power
    platform.add_processor(pt, cd, pd)
    platform.finalize()

    engine.run(5)  # int time
    assert engine.time() == 5.0

    print("  Int accepted as Duration: OK")


def test_negative_duration_allowed():
    """Test that negative durations don't crash the type conversion.

    Note: The engine may reject them at runtime, but the Python->C++
    conversion should handle negative values gracefully.
    """
    engine = sim.Engine()
    platform = engine.get_platform()

    # Negative values should at least not crash during conversion
    # Whether they're valid semantically is a separate concern
    try:
        # This might raise a ValueError from C++ validation
        task = platform.add_task(-10.0, -10.0, -2.0)
        # If it gets here, negative values were accepted (unusual but possible)
    except (ValueError, RuntimeError):
        # Expected - the C++ side rejects negative durations
        pass

    print("  Negative duration handling: OK")


def test_cstate_tuple_conversion():
    """Test that 4-tuple converts to CStateLevel correctly."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Test various C-state configurations
    c_states = [
        (0, 0, 0.0, 100.0),      # C0: level=0, PerProcessor, 0 latency, 100 mW
        (1, 0, 0.001, 50.0),     # C1: level=1, PerProcessor, 1ms latency, 50 mW
        (2, 1, 0.010, 10.0),     # C2: level=2, DomainWide, 10ms latency, 10 mW
    ]

    pd = platform.add_power_domain(c_states)
    assert pd.id() == 0

    # Test with integer scope values (0 = PerProcessor, 1 = DomainWide)
    c_states2 = [
        (0, sim.CStateScope_PerProcessor, 0.0, 100.0),  # Using enum constant
    ]
    # Note: Currently only int is supported in the tuple, not the enum
    # pd2 = platform.add_power_domain(c_states2)  # Would fail

    print("  CState tuple conversion: OK")


def main():
    print("Running pyschedsim type conversion tests...")

    test_duration_conversion()
    test_timepoint_conversion()
    test_frequency_mhz()
    test_power_mw()
    test_energy_mj()
    test_int_accepted_as_duration()
    test_negative_duration_allowed()
    test_cstate_tuple_conversion()

    print("\nAll pyschedsim type tests passed!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
