#!/usr/bin/env python3
"""Edge case tests for pyschedsim - validates boundary conditions and unusual scenarios."""

import sys

try:
    import pyschedsim as sim
except Exception as e:
    raise SystemExit(f"Failed to import pyschedsim: {e}")


def test_empty_platform_no_processors():
    """Test that a platform with no processors can be finalized."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Don't add any processors - just processor type, clock domain, power domain
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])

    # Finalize with no processors
    platform.finalize()

    assert platform.processor_count() == 0
    assert platform.is_finalized()

    # Running should still work (nothing to do)
    engine.run(10.0)
    assert engine.time() == 10.0

    print("  Empty platform (no processors): OK")


def test_platform_no_tasks():
    """Test running a simulation with no tasks."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Build platform with processor but no tasks
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    proc = platform.add_processor(pt, cd, pd)
    platform.finalize()

    assert platform.task_count() == 0

    # Create scheduler with no tasks
    scheduler = sim.EdfScheduler(engine, [proc])
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)

    # Run - should complete without error
    engine.run(10.0)
    assert engine.time() == 10.0

    print("  Platform with no tasks: OK")


def test_many_processors():
    """Test platform with many processors (20+)."""
    engine = sim.Engine()
    platform = engine.get_platform()

    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])

    num_procs = 24  # More than typical multicore
    for _ in range(num_procs):
        platform.add_processor(pt, cd, pd)

    platform.finalize()

    assert platform.processor_count() == num_procs, \
        f"Expected {num_procs} processors, got {platform.processor_count()}"

    # Verify all processors are accessible
    procs = sim.get_all_processors(engine)
    assert len(procs) == num_procs

    for i, proc in enumerate(procs):
        assert proc.id() == i, f"Expected processor id={i}, got {proc.id()}"

    print("  Many processors (24): OK")


def test_wcet_equals_deadline():
    """Test task where WCET equals relative deadline (tight constraint)."""
    engine = sim.Engine()
    platform = engine.get_platform()

    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    proc = platform.add_processor(pt, cd, pd)

    # WCET == deadline (tight constraint)
    task = platform.add_task(10.0, 2.0, 2.0)  # period=10, deadline=2, wcet=2
    platform.finalize()

    assert task.wcet() == task.relative_deadline()

    # Should be schedulable (u = 2/10 = 0.2)
    scheduler = sim.EdfScheduler(engine, [proc])
    assert scheduler.can_admit(2.0, 10.0)
    scheduler.add_server(task)

    print("  WCET equals deadline: OK")


def test_wcet_equals_period():
    """Test task where WCET equals period (utilization = 1.0)."""
    engine = sim.Engine()
    platform = engine.get_platform()

    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    proc = platform.add_processor(pt, cd, pd)

    # WCET == period (100% utilization)
    task = platform.add_task(10.0, 10.0, 10.0)  # period=10, deadline=10, wcet=10
    platform.finalize()

    assert task.wcet() == task.period()

    # Single task with u=1.0 should be admissible on single processor
    scheduler = sim.EdfScheduler(engine, [proc])
    assert scheduler.can_admit(10.0, 10.0)
    scheduler.add_server(task)

    assert scheduler.utilization() == 1.0, \
        f"Expected utilization=1.0, got {scheduler.utilization()}"

    print("  WCET equals period: OK")


def test_run_with_no_jobs():
    """Test running with tasks but no scheduled jobs."""
    engine = sim.Engine()
    platform = engine.get_platform()

    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    proc = platform.add_processor(pt, cd, pd)

    # Add task but don't schedule any jobs
    task = platform.add_task(10.0, 10.0, 2.0)
    platform.finalize()

    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.add_server(task)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)

    # Don't schedule any job arrivals
    # Run should complete with processor idle the whole time
    engine.run(50.0)

    assert engine.time() == 50.0
    assert proc.state() == sim.ProcessorState_Idle

    print("  Run with no jobs: OK")


def test_run_empty_queue():
    """Test run() without time limit when event queue is empty."""
    engine = sim.Engine()
    platform = engine.get_platform()

    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    platform.add_processor(pt, cd, pd)
    platform.finalize()

    # Call run() without arguments - should return immediately with empty queue
    engine.run()

    # Time should still be 0 since nothing happened
    assert engine.time() == 0.0, f"Expected time=0.0, got {engine.time()}"

    print("  Run empty queue: OK")


def test_multiple_runs():
    """Test calling run() multiple times with increasing time limits."""
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

    # Schedule multiple jobs
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.schedule_job_arrival(task, 10.0, 2.0)
    engine.schedule_job_arrival(task, 20.0, 2.0)

    # Run in increments
    engine.run(5.0)
    assert engine.time() == 5.0, f"After run(5): expected 5.0, got {engine.time()}"

    engine.run(15.0)
    assert engine.time() == 15.0, f"After run(15): expected 15.0, got {engine.time()}"

    engine.run(25.0)
    assert engine.time() == 25.0, f"After run(25): expected 25.0, got {engine.time()}"

    print("  Multiple runs: OK")


def test_run_past_completion():
    """Test running far past when all jobs complete."""
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

    # Schedule a single job at t=0 with 2s execution time
    engine.schedule_job_arrival(task, 0.0, 2.0)

    # Run way past completion (job finishes at t=2)
    engine.run(1000.0)

    # Should reach the target time
    assert engine.time() == 1000.0, f"Expected time=1000.0, got {engine.time()}"

    # Processor should be idle (no more work)
    assert proc.state() == sim.ProcessorState_Idle

    print("  Run past completion: OK")


def main():
    print("Running pyschedsim edge case tests...")

    test_empty_platform_no_processors()
    test_platform_no_tasks()
    test_many_processors()
    test_wcet_equals_deadline()
    test_wcet_equals_period()
    test_run_with_no_jobs()
    test_run_empty_queue()
    test_multiple_runs()
    test_run_past_completion()

    print("\nAll pyschedsim edge case tests passed!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
