#!/usr/bin/env python3
"""MVP tests for pyschedsim core functionality."""

import sys

# Expect PYTHONPATH to include the build/python directory (set by CTest)
try:
    import pyschedsim as sim
except Exception as e:
    raise SystemExit(f"Failed to import pyschedsim: {e}")


def test_module_exports():
    """Test that essential classes and functions are exported."""
    # Core classes
    assert hasattr(sim, "Engine"), "Engine not exported"
    assert hasattr(sim, "Platform"), "Platform not exported"
    assert hasattr(sim, "Processor"), "Processor not exported"
    assert hasattr(sim, "ProcessorType"), "ProcessorType not exported"
    assert hasattr(sim, "ClockDomain"), "ClockDomain not exported"
    assert hasattr(sim, "PowerDomain"), "PowerDomain not exported"
    assert hasattr(sim, "Task"), "Task not exported"

    # Algo classes
    assert hasattr(sim, "EdfScheduler"), "EdfScheduler not exported"
    assert hasattr(sim, "SingleSchedulerAllocator"), "SingleSchedulerAllocator not exported"

    # I/O functions
    assert hasattr(sim, "load_platform"), "load_platform not exported"
    assert hasattr(sim, "load_scenario"), "load_scenario not exported"
    assert hasattr(sim, "inject_scenario"), "inject_scenario not exported"
    assert hasattr(sim, "schedule_arrivals"), "schedule_arrivals not exported"

    # Enums
    assert hasattr(sim, "ProcessorState_Idle"), "ProcessorState enum not exported"
    assert hasattr(sim, "CStateScope_PerProcessor"), "CStateScope enum not exported"

    print("  Module exports: OK")


def test_engine_creation():
    """Test basic Engine creation and methods."""
    engine = sim.Engine()

    # Check initial state
    assert engine.time() == 0.0, f"Expected time=0, got {engine.time()}"
    assert not engine.is_finalized(), "Engine should not be finalized initially"

    print("  Engine creation: OK")


def test_platform_building():
    """Test building a platform from scratch."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Add a processor type
    pt = platform.add_processor_type("cpu", 1.0)
    assert pt.id() == 0, f"Expected processor type id=0, got {pt.id()}"
    assert pt.performance() == 1.0, f"Expected performance=1.0, got {pt.performance()}"

    # Add a clock domain (freq_min, freq_max in MHz)
    cd = platform.add_clock_domain(500.0, 2000.0)
    assert cd.id() == 0, f"Expected clock domain id=0, got {cd.id()}"
    assert cd.freq_min() == 500.0, f"Expected freq_min=500, got {cd.freq_min()}"
    assert cd.freq_max() == 2000.0, f"Expected freq_max=2000, got {cd.freq_max()}"
    assert cd.frequency() == 2000.0, "Initial frequency should be freq_max"

    # Add a power domain with C-states
    # Each tuple: (level, scope, wake_latency, power)
    # scope: 0 = PerProcessor, 1 = DomainWide
    c_states = [
        (0, 0, 0.0, 100.0),   # C0 - active state
        (1, 0, 0.001, 10.0),  # C1 - light sleep
    ]
    pd = platform.add_power_domain(c_states)
    assert pd.id() == 0, f"Expected power domain id=0, got {pd.id()}"

    # Add a processor
    proc = platform.add_processor(pt, cd, pd)
    assert proc.id() == 0, f"Expected processor id=0, got {proc.id()}"
    assert proc.state() == sim.ProcessorState_Idle, "Processor should be Idle initially"

    # Add a task (period, deadline, wcet in seconds)
    task = platform.add_task(10.0, 10.0, 2.0)
    assert task.id() == 0, f"Expected task id=0, got {task.id()}"
    assert task.period() == 10.0, f"Expected period=10.0, got {task.period()}"
    assert task.relative_deadline() == 10.0, f"Expected deadline=10.0, got {task.relative_deadline()}"
    assert task.wcet() == 2.0, f"Expected wcet=2.0, got {task.wcet()}"

    # Check counts
    assert platform.processor_type_count() == 1
    assert platform.clock_domain_count() == 1
    assert platform.power_domain_count() == 1
    assert platform.processor_count() == 1
    assert platform.task_count() == 1

    print("  Platform building: OK")


def test_finalization():
    """Test platform finalization."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Build minimal platform
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    platform.add_processor(pt, cd, pd)

    assert not platform.is_finalized(), "Platform should not be finalized yet"

    # Finalize platform
    platform.finalize()

    assert platform.is_finalized(), "Platform should be finalized"
    # Note: Engine is finalized when platform.finalize() is called
    # because Platform::finalize() internally calls engine_.finalize()
    # Let's check actual behavior
    # Actually checking the engine code - it doesn't auto-finalize engine
    # so just checking platform is finalized is enough

    print("  Finalization: OK")


def test_simple_run():
    """Test running the engine without any events."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Build minimal platform
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    platform.add_processor(pt, cd, pd)
    platform.finalize()

    # Run until time 10
    engine.run(10.0)

    assert engine.time() == 10.0, f"Expected time=10.0, got {engine.time()}"

    print("  Simple run: OK")


def test_edf_scheduler_creation():
    """Test creating an EDF scheduler."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Build platform with 2 processors
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    platform.add_processor(pt, cd, pd)
    platform.add_processor(pt, cd, pd)
    platform.finalize()

    # Get processors list
    procs = sim.get_all_processors(engine)
    assert len(procs) == 2, f"Expected 2 processors, got {len(procs)}"

    # Create scheduler
    scheduler = sim.EdfScheduler(engine, procs)

    assert scheduler.processor_count() == 2, f"Expected 2 processors in scheduler, got {scheduler.processor_count()}"
    assert scheduler.server_count() == 0, "No servers added yet"
    assert scheduler.utilization() == 0.0, f"Expected utilization=0, got {scheduler.utilization()}"

    print("  EDF scheduler creation: OK")


def test_full_simulation():
    """Test a complete EDF simulation with job arrivals."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Build platform
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    proc = platform.add_processor(pt, cd, pd)

    # Add a task: period=10s, deadline=10s, wcet=2s
    task = platform.add_task(10.0, 10.0, 2.0)
    platform.finalize()

    # Create scheduler with the processor
    procs = [proc]
    scheduler = sim.EdfScheduler(engine, procs)

    # Add server for the task (uses task's wcet/period by default)
    scheduler.add_server(task)
    assert scheduler.server_count() == 1, "Should have 1 server"

    # Create allocator (this sets the job arrival handler)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)

    # Schedule job arrivals
    engine.schedule_job_arrival(task, 0.0, 2.0)   # Job at t=0, exec_time=2s
    engine.schedule_job_arrival(task, 10.0, 2.0)  # Job at t=10, exec_time=2s

    # Run simulation
    engine.run(25.0)

    assert engine.time() >= 25.0, f"Expected time >= 25, got {engine.time()}"

    print("  Full simulation: OK")


def test_admission_control():
    """Test that admission control works."""
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
    assert scheduler.can_admit(8.0, 10.0), "Should be able to admit first server"
    scheduler.add_server(task1)

    # Second server should NOT be admitted (0.8 + 0.5 > 1.0)
    assert not scheduler.can_admit(5.0, 10.0), "Should not be able to admit second server"

    try:
        scheduler.add_server(task2)
        assert False, "Should have raised exception for over-utilization"
    except Exception as e:
        # Expected - admission error
        pass

    print("  Admission control: OK")


def test_helper_functions():
    """Test Python helper functions."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Build platform
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    platform.add_processor(pt, cd, pd)
    platform.add_processor(pt, cd, pd)
    platform.add_task(10.0, 10.0, 2.0)
    platform.add_task(5.0, 5.0, 1.0)
    platform.finalize()

    # Test get_all_processors
    procs = sim.get_all_processors(engine)
    assert len(procs) == 2, f"Expected 2 processors, got {len(procs)}"
    assert procs[0].id() == 0
    assert procs[1].id() == 1

    # Test get_all_tasks
    tasks = sim.get_all_tasks(engine)
    assert len(tasks) == 2, f"Expected 2 tasks, got {len(tasks)}"
    assert tasks[0].id() == 0
    assert tasks[1].id() == 1

    print("  Helper functions: OK")


def test_context_switch_enable():
    """Test enabling context switch simulation."""
    engine = sim.Engine()

    # Enable context switching
    engine.enable_context_switch(True)
    assert engine.context_switch_enabled(), "Context switch should be enabled"

    # Disable
    engine.enable_context_switch(False)
    assert not engine.context_switch_enabled(), "Context switch should be disabled"

    print("  Context switch enable: OK")


def test_context_switch_disabled_by_default():
    """Test that context switching is disabled by default."""
    engine = sim.Engine()

    # Should be disabled by default (for performance)
    assert not engine.context_switch_enabled(), \
        "Context switch should be disabled by default"

    print("  Context switch disabled by default: OK")


def test_run_unlimited():
    """Test run() without time limit (runs until event queue is empty)."""
    engine = sim.Engine()
    platform = engine.get_platform()

    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    proc = platform.add_processor(pt, cd, pd)
    task = platform.add_task(10.0, 10.0, 2.0)
    platform.finalize()

    # Set up scheduler
    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.add_server(task)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)

    # Schedule a single job
    engine.schedule_job_arrival(task, 0.0, 2.0)

    # Run without time limit - should stop when queue is empty
    engine.run()

    # After run(), time should be at least when job completed (t=2)
    assert engine.time() >= 2.0, f"Expected time >= 2.0, got {engine.time()}"

    print("  Run unlimited: OK")


def test_clock_domain_properties():
    """Test ClockDomain property accessors."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Create clock domain
    cd = platform.add_clock_domain(500.0, 2000.0)

    # Test properties
    assert cd.freq_min() == 500.0, f"Expected freq_min=500, got {cd.freq_min()}"
    assert cd.freq_max() == 2000.0, f"Expected freq_max=2000, got {cd.freq_max()}"
    assert cd.frequency() == 2000.0, f"Expected frequency=2000 (starts at max), got {cd.frequency()}"

    # State properties
    assert not cd.is_locked(), "Clock domain should not be locked initially"
    assert not cd.is_transitioning(), "Clock domain should not be transitioning initially"

    print("  Clock domain properties: OK")


def test_processor_type_properties():
    """Test ProcessorType property accessors."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Create processor types with different performance values
    pt1 = platform.add_processor_type("big", 2.0)
    pt2 = platform.add_processor_type("little", 0.5)

    assert pt1.id() == 0
    assert pt1.performance() == 2.0, f"Expected performance=2.0, got {pt1.performance()}"

    assert pt2.id() == 1
    assert pt2.performance() == 0.5, f"Expected performance=0.5, got {pt2.performance()}"

    print("  Processor type properties: OK")


def test_reference_performance():
    """Test Platform.reference_performance()."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Create heterogeneous platform
    pt_big = platform.add_processor_type("big", 2.0)
    pt_little = platform.add_processor_type("little", 0.5)

    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])

    platform.add_processor(pt_big, cd, pd)
    platform.add_processor(pt_little, cd, pd)
    platform.finalize()

    # Reference performance is typically set to some baseline
    ref_perf = platform.reference_performance()
    assert isinstance(ref_perf, float), f"Expected float, got {type(ref_perf)}"
    assert ref_perf > 0.0, f"Reference performance should be positive, got {ref_perf}"

    print("  Reference performance: OK")


def test_enum_values_exist():
    """Test that all enum values are exported and accessible."""
    # ProcessorState enum values
    assert hasattr(sim, 'ProcessorState_Idle'), "ProcessorState_Idle not exported"
    assert hasattr(sim, 'ProcessorState_ContextSwitching'), "ProcessorState_ContextSwitching not exported"
    assert hasattr(sim, 'ProcessorState_Running'), "ProcessorState_Running not exported"
    assert hasattr(sim, 'ProcessorState_Sleep'), "ProcessorState_Sleep not exported"
    assert hasattr(sim, 'ProcessorState_Changing'), "ProcessorState_Changing not exported"

    # CStateScope enum values
    assert hasattr(sim, 'CStateScope_PerProcessor'), "CStateScope_PerProcessor not exported"
    assert hasattr(sim, 'CStateScope_DomainWide'), "CStateScope_DomainWide not exported"

    # DeadlineMissPolicy enum values
    assert hasattr(sim, 'DeadlineMissPolicy_Continue'), "DeadlineMissPolicy_Continue not exported"
    assert hasattr(sim, 'DeadlineMissPolicy_AbortJob'), "DeadlineMissPolicy_AbortJob not exported"
    assert hasattr(sim, 'DeadlineMissPolicy_AbortTask'), "DeadlineMissPolicy_AbortTask not exported"
    assert hasattr(sim, 'DeadlineMissPolicy_StopSimulation'), "DeadlineMissPolicy_StopSimulation not exported"

    # Convenience constants
    assert hasattr(sim, 'PROC_IDLE'), "PROC_IDLE convenience constant not exported"
    assert hasattr(sim, 'CSTATE_PER_PROCESSOR'), "CSTATE_PER_PROCESSOR convenience constant not exported"

    print("  Enum values exist: OK")


def main():
    print("Running pyschedsim core tests...")

    test_module_exports()
    test_engine_creation()
    test_platform_building()
    test_finalization()
    test_simple_run()
    test_edf_scheduler_creation()
    test_full_simulation()
    test_admission_control()
    test_helper_functions()
    test_context_switch_enable()
    test_context_switch_disabled_by_default()
    test_run_unlimited()
    test_clock_domain_properties()
    test_processor_type_properties()
    test_reference_performance()
    test_enum_values_exist()

    print("\nAll pyschedsim core tests passed!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
