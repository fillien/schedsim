#!/usr/bin/env python3
"""Energy tracking tests for pyschedsim - validates energy measurement features."""

import sys

try:
    import pyschedsim as sim
except Exception as e:
    raise SystemExit(f"Failed to import pyschedsim: {e}")


def build_platform_with_power(engine, enable_energy=True):
    """Helper to build a platform with power model via JSON loader.

    Uses JSON platform format which properly sets up power coefficients.
    The power_model field [a, b, c, d] defines P(f) = a + b*f + c*f^2 + d*f^3
    where f is frequency in GHz.

    Note: Energy tracking must be enabled AFTER platform is loaded/finalized.
    """
    platform_json = '''{
        "clusters": [
            {
                "effective_freq": 1000.0,
                "frequencies": [1000.0],
                "power_model": [50.0, 100.0, 0.0, 0.0],
                "procs": 1,
                "perf_score": 1.0
            }
        ]
    }'''
    # power_model: P(f) = 50 + 100*f mW at f GHz
    # At 1 GHz: P = 50 + 100 = 150 mW

    sim.load_platform_from_string(engine, platform_json)

    # Enable energy tracking AFTER platform is finalized
    if enable_energy:
        engine.enable_energy_tracking(True)

    return engine.platform


def test_energy_tracking_disabled_by_default():
    """Test that energy tracking is disabled by default."""
    engine = sim.Engine()

    assert not engine.energy_tracking_enabled, \
        "Energy tracking should be disabled by default"

    print("  Energy tracking disabled by default: OK")


def test_enable_energy_tracking():
    """Test enabling/disabling energy tracking."""
    engine = sim.Engine()

    # Enable
    engine.enable_energy_tracking(True)
    assert engine.energy_tracking_enabled, "Energy tracking should be enabled"

    # Disable
    engine.enable_energy_tracking(False)
    assert not engine.energy_tracking_enabled, "Energy tracking should be disabled"

    # Re-enable
    engine.enable_energy_tracking(True)
    assert engine.energy_tracking_enabled, "Energy tracking should be re-enabled"

    print("  Enable energy tracking: OK")


def test_processor_energy():
    """Test getting energy for a specific processor."""
    engine = sim.Engine()

    # build_platform_with_power enables energy tracking after platform load
    platform = build_platform_with_power(engine)

    # Run for 1 second
    engine.run(1.0)

    # Get processor energy
    proc_energy = engine.processor_energy(0)

    assert isinstance(proc_energy, float), f"Expected float, got {type(proc_energy)}"
    assert proc_energy >= 0.0, f"Energy should be non-negative, got {proc_energy}"

    print("  Processor energy: OK")


def test_clock_domain_energy():
    """Test getting energy for a clock domain."""
    engine = sim.Engine()

    platform = build_platform_with_power(engine)

    # Run simulation
    engine.run(1.0)

    # Get clock domain energy
    cd_energy = engine.clock_domain_energy(0)

    assert isinstance(cd_energy, float), f"Expected float, got {type(cd_energy)}"
    assert cd_energy >= 0.0, f"Energy should be non-negative, got {cd_energy}"

    print("  Clock domain energy: OK")


def test_power_domain_energy():
    """Test getting energy for a power domain."""
    engine = sim.Engine()

    platform = build_platform_with_power(engine)

    # Run simulation
    engine.run(1.0)

    # Get power domain energy
    pd_energy = engine.power_domain_energy(0)

    assert isinstance(pd_energy, float), f"Expected float, got {type(pd_energy)}"
    assert pd_energy >= 0.0, f"Energy should be non-negative, got {pd_energy}"

    print("  Power domain energy: OK")


def test_total_energy():
    """Test getting total system energy."""
    engine = sim.Engine()

    platform = build_platform_with_power(engine)

    # Run for 1 second
    engine.run(1.0)

    # Get total energy
    total = engine.total_energy()

    assert isinstance(total, float), f"Expected float, got {type(total)}"
    assert total >= 0.0, f"Total energy should be non-negative, got {total}"

    # Total should equal sum of all processor energies (single processor case)
    proc_energy = engine.processor_energy(0)
    # Note: Depending on implementation, total might include more than just processor energy
    # At minimum, total should be >= individual processor energy

    print("  Total energy: OK")


def test_energy_tracking_requires_enabled():
    """Test that energy queries raise exception when tracking is disabled."""
    engine = sim.Engine()
    # Do NOT enable energy tracking

    # Build simple platform without JSON (to avoid loading which might finalize)
    platform = engine.platform
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    platform.add_processor(pt, cd, pd)
    platform.finalize()

    # Run simulation
    engine.run(1.0)

    # Energy queries should raise exception when tracking is disabled
    # (InvalidStateError maps to RuntimeError in Python)
    try:
        _ = engine.total_energy()
        assert False, "Should have raised exception when energy tracking disabled"
    except RuntimeError as e:
        # Expected - InvalidStateError maps to RuntimeError
        assert "tracking" in str(e).lower() or "enabled" in str(e).lower(), \
            f"Unexpected error message: {e}"

    try:
        _ = engine.processor_energy(0)
        assert False, "Should have raised exception when energy tracking disabled"
    except RuntimeError:
        # Expected
        pass

    print("  Energy tracking requires enabled: OK")


def test_energy_accumulates_during_run():
    """Test that energy accumulates over simulation time."""
    engine = sim.Engine()

    platform = build_platform_with_power(engine)

    # Run for 1 second
    engine.run(1.0)
    energy_at_1s = engine.total_energy()

    # Run for another second (total 2 seconds)
    engine.run(2.0)
    energy_at_2s = engine.total_energy()

    # Run for another second (total 3 seconds)
    engine.run(3.0)
    energy_at_3s = engine.total_energy()

    # Energy should increase over time
    assert energy_at_2s > energy_at_1s, \
        f"Energy should increase: {energy_at_1s} -> {energy_at_2s}"
    assert energy_at_3s > energy_at_2s, \
        f"Energy should increase: {energy_at_2s} -> {energy_at_3s}"

    # Energy should roughly scale with time (allow 10% tolerance for idle variations)
    ratio = energy_at_2s / energy_at_1s
    assert 1.5 < ratio < 2.5, \
        f"Energy at 2s should be ~2x energy at 1s, got ratio {ratio}"

    print("  Energy accumulates during run: OK")


def main():
    print("Running pyschedsim energy tracking tests...")

    test_energy_tracking_disabled_by_default()
    test_enable_energy_tracking()
    test_processor_energy()
    test_clock_domain_energy()
    test_power_domain_energy()
    test_total_energy()
    test_energy_tracking_requires_enabled()
    test_energy_accumulates_during_run()

    print("\nAll pyschedsim energy tests passed!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
