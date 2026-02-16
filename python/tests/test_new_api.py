#!/usr/bin/env python3
"""Tests for new Python API features: policies, trace writers, metrics, cluster."""

import os
import sys
import tempfile

try:
    import pyschedsim as sim
except Exception as e:
    raise SystemExit(f"Failed to import pyschedsim: {e}")


def make_simple_engine():
    """Create a simple engine with 1 processor and 1 task."""
    engine = sim.Engine()
    platform = engine.get_platform()
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    proc = platform.add_processor(pt, cd, pd)
    task = platform.add_task(10.0, 10.0, 2.0)
    platform.finalize()
    return engine, proc, task


def test_new_exports():
    """Test that new classes and functions are exported."""
    # Trace writers
    assert hasattr(sim, "NullTraceWriter"), "NullTraceWriter not exported"
    assert hasattr(sim, "MemoryTraceWriter"), "MemoryTraceWriter not exported"
    assert hasattr(sim, "FileJsonTraceWriter"), "FileJsonTraceWriter not exported"
    assert hasattr(sim, "TraceWriter"), "TraceWriter not exported"
    assert hasattr(sim, "TraceRecord"), "TraceRecord not exported"

    # Metrics
    assert hasattr(sim, "SimulationMetrics"), "SimulationMetrics not exported"
    assert hasattr(sim, "ResponseTimeStats"), "ResponseTimeStats not exported"
    assert hasattr(sim, "compute_metrics_from_file"), "compute_metrics_from_file not exported"

    # Cluster
    assert hasattr(sim, "Cluster"), "Cluster not exported"

    # Time-series structs
    assert hasattr(sim, "FrequencyInterval"), "FrequencyInterval not exported"
    assert hasattr(sim, "CoreCountInterval"), "CoreCountInterval not exported"
    assert hasattr(sim, "ConfigInterval"), "ConfigInterval not exported"

    # DeadlineMissPolicy convenience constants
    assert hasattr(sim, "DM_CONTINUE"), "DM_CONTINUE not exported"
    assert hasattr(sim, "DM_ABORT_JOB"), "DM_ABORT_JOB not exported"
    assert hasattr(sim, "DM_STOP_SIMULATION"), "DM_STOP_SIMULATION not exported"

    print("  New exports: OK")


def test_add_task_with_id():
    """Test Platform.add_task with explicit ID."""
    engine = sim.Engine()
    platform = engine.get_platform()

    # Add task with explicit ID
    task = platform.add_task(42, 10.0, 10.0, 2.0)
    assert task.id() == 42, f"Expected task id=42, got {task.id()}"

    # Add another task without explicit ID (auto-assigned)
    task2 = platform.add_task(5.0, 5.0, 1.0)
    # Auto-assigned IDs start from 0 but skip used ones
    assert task2.period() == 5.0

    print("  add_task with explicit ID: OK")


def test_null_trace_writer():
    """Test NullTraceWriter can be created and set."""
    engine, proc, task = make_simple_engine()

    writer = sim.NullTraceWriter()
    engine.set_trace_writer(writer)

    # Should not crash
    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.add_server(task)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.run(5.0)

    # Disable tracing
    engine.set_trace_writer(None)

    print("  NullTraceWriter: OK")


def test_memory_trace_writer():
    """Test MemoryTraceWriter collects trace records."""
    engine, proc, task = make_simple_engine()

    writer = sim.MemoryTraceWriter()
    engine.set_trace_writer(writer)

    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.add_server(task)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.run(5.0)

    # Should have recorded some trace events
    count = writer.record_count()
    assert count > 0, f"Expected some trace records, got {count}"

    # Check first record
    rec = writer.record(0)
    assert isinstance(rec.time, float), f"Expected float time, got {type(rec.time)}"
    assert isinstance(rec.type, str), f"Expected string type, got {type(rec.type)}"

    # Check field access
    fields_dict = rec.get_fields_dict()
    assert isinstance(fields_dict, dict), f"Expected dict, got {type(fields_dict)}"

    field_names = rec.get_field_names()
    assert isinstance(field_names, list), f"Expected list, got {type(field_names)}"

    # Test get_field for a known field
    for name in field_names:
        val = rec.get_field(name)
        assert val is not None, f"Field '{name}' should not be None"

    # Test get_field for unknown field returns None
    val = rec.get_field("nonexistent_field_xyz")
    assert val is None, "Expected None for nonexistent field"

    # Test clear
    writer.clear()
    assert writer.record_count() == 0, "After clear, record count should be 0"

    print("  MemoryTraceWriter: OK")


def test_memory_trace_writer_compute_metrics():
    """Test computing metrics from MemoryTraceWriter."""
    engine, proc, task = make_simple_engine()

    writer = sim.MemoryTraceWriter()
    engine.set_trace_writer(writer)

    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.add_server(task)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.schedule_job_arrival(task, 10.0, 1.5)
    engine.run(25.0)

    # Compute metrics from the writer's traces
    metrics = writer.compute_metrics()
    assert isinstance(metrics, sim.SimulationMetrics)
    assert metrics.total_jobs >= 2, f"Expected at least 2 jobs, got {metrics.total_jobs}"
    assert metrics.completed_jobs >= 2, f"Expected at least 2 completed, got {metrics.completed_jobs}"
    assert metrics.deadline_misses == 0, f"Expected 0 deadline misses, got {metrics.deadline_misses}"

    # Test map accessor methods
    util_dict = metrics.get_utilization_per_processor()
    assert isinstance(util_dict, dict), f"Expected dict, got {type(util_dict)}"

    rt_dict = metrics.get_response_times_per_task()
    assert isinstance(rt_dict, dict), f"Expected dict, got {type(rt_dict)}"

    dm_dict = metrics.get_deadline_misses_per_task()
    assert isinstance(dm_dict, dict), f"Expected dict, got {type(dm_dict)}"

    print("  MemoryTraceWriter.compute_metrics: OK")


def test_file_json_trace_writer():
    """Test FileJsonTraceWriter writes JSON to file."""
    engine, proc, task = make_simple_engine()

    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as f:
        tmpfile = f.name

    try:
        writer = sim.FileJsonTraceWriter(tmpfile)
        engine.set_trace_writer(writer)

        scheduler = sim.EdfScheduler(engine, [proc])
        scheduler.add_server(task)
        allocator = sim.SingleSchedulerAllocator(engine, scheduler)
        engine.schedule_job_arrival(task, 0.0, 2.0)
        engine.run(5.0)

        # Finalize the writer to close the JSON array
        writer.finalize()
        engine.set_trace_writer(None)

        # Verify file was written
        size = os.path.getsize(tmpfile)
        assert size > 0, f"Expected non-empty file, got {size} bytes"

        # Verify it's valid JSON
        import json
        with open(tmpfile) as f:
            data = json.load(f)
        assert isinstance(data, list), f"Expected JSON array, got {type(data)}"
        assert len(data) > 0, "Expected at least one trace record"
        assert "type" in data[0], "Expected 'type' field in trace record"
    finally:
        os.unlink(tmpfile)

    print("  FileJsonTraceWriter: OK")


def test_enable_grub():
    """Test enabling GRUB reclamation policy."""
    engine, proc, task = make_simple_engine()

    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.enable_grub()  # Should not throw
    scheduler.add_server(task)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.run(5.0)

    print("  enable_grub: OK")


def test_enable_cash():
    """Test enabling CASH reclamation policy."""
    engine, proc, task = make_simple_engine()

    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.enable_cash()  # Should not throw
    scheduler.add_server(task)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.run(5.0)

    print("  enable_cash: OK")


def test_enable_dvfs_policies():
    """Test enabling DVFS policies."""
    engine = sim.Engine()
    platform = engine.get_platform()
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(500.0, 2000.0, 0.001)  # With transition delay
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0), (1, 0, 0.001, 10.0)])
    proc = platform.add_processor(pt, cd, pd)
    task = platform.add_task(10.0, 10.0, 2.0)
    platform.finalize()

    scheduler = sim.EdfScheduler(engine, [proc])

    # Enable GRUB (required for DVFS)
    scheduler.enable_grub()

    # Enable PA DVFS with cooldown
    scheduler.enable_power_aware_dvfs(0.01)

    scheduler.add_server(task)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.run(5.0)

    print("  enable_dvfs_policies: OK")


def test_enable_dpm():
    """Test enabling DPM policy."""
    engine = sim.Engine()
    platform = engine.get_platform()
    pt = platform.add_processor_type("cpu", 1.0)
    cd = platform.add_clock_domain(1000.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0), (1, 0, 0.001, 10.0)])
    proc = platform.add_processor(pt, cd, pd)
    task = platform.add_task(10.0, 10.0, 2.0)
    platform.finalize()

    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.enable_basic_dpm(1)  # Target C-state 1
    scheduler.add_server(task)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.run(5.0)

    print("  enable_basic_dpm: OK")


def test_set_deadline_miss_policy():
    """Test setting deadline miss policy."""
    engine, proc, task = make_simple_engine()

    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.set_deadline_miss_policy(sim.DeadlineMissPolicy_StopSimulation)
    scheduler.add_server(task)

    print("  set_deadline_miss_policy: OK")


def test_set_expected_arrivals():
    """Test setting expected arrivals for M-GRUB."""
    engine, proc, task = make_simple_engine()

    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.enable_grub()
    scheduler.add_server(task)

    # Set expected arrivals for M-GRUB server detach
    scheduler.set_expected_arrivals(task, 3)

    allocator = sim.SingleSchedulerAllocator(engine, scheduler)
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.schedule_job_arrival(task, 10.0, 1.5)
    engine.schedule_job_arrival(task, 20.0, 1.0)
    engine.run(35.0)

    print("  set_expected_arrivals: OK")


def test_utilization_queries():
    """Test utilization query methods."""
    engine, proc, task = make_simple_engine()

    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.enable_grub()
    scheduler.add_server(task)

    allocator = sim.SingleSchedulerAllocator(engine, scheduler)
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.run(1.0)  # Run partway through

    # Query utilization
    u = scheduler.utilization()
    assert isinstance(u, float), f"Expected float, got {type(u)}"
    assert u >= 0.0, f"Utilization should be >= 0, got {u}"

    au = scheduler.active_utilization()
    assert isinstance(au, float), f"Expected float, got {type(au)}"

    su = scheduler.scheduler_utilization()
    assert isinstance(su, float), f"Expected float, got {type(su)}"

    msu = scheduler.max_scheduler_utilization()
    assert isinstance(msu, float), f"Expected float, got {type(msu)}"

    print("  Utilization queries: OK")


def test_cluster_class():
    """Test Cluster class."""
    engine = sim.Engine()
    platform = engine.get_platform()

    pt = platform.add_processor_type("big", 2.0)
    cd = platform.add_clock_domain(500.0, 2000.0)
    pd = platform.add_power_domain([(0, 0, 0.0, 100.0)])
    proc = platform.add_processor(pt, cd, pd)
    task = platform.add_task(10.0, 10.0, 2.0)
    platform.finalize()

    scheduler = sim.EdfScheduler(engine, [proc])

    # Create a cluster
    cluster = sim.Cluster(cd, scheduler, 2.0, 2000.0)

    assert cluster.perf() == 2.0, f"Expected perf=2.0, got {cluster.perf()}"
    assert cluster.processor_count() == 1, f"Expected 1 proc, got {cluster.processor_count()}"
    assert cluster.u_target() == 1.0, f"Expected default u_target=1.0, got {cluster.u_target()}"

    # Test setters
    cluster.set_u_target(0.8)
    assert cluster.u_target() == 0.8, f"Expected u_target=0.8, got {cluster.u_target()}"

    # Test scaled_utilization
    su = cluster.scaled_utilization(0.5)
    assert isinstance(su, float), f"Expected float, got {type(su)}"

    # Test can_admit (should be able to admit since scheduler is empty)
    assert cluster.can_admit(2.0, 10.0), "Should be able to admit"

    print("  Cluster class: OK")


def test_simulation_metrics_struct():
    """Test SimulationMetrics struct fields."""
    m = sim.SimulationMetrics()
    assert m.total_jobs == 0
    assert m.completed_jobs == 0
    assert m.deadline_misses == 0
    assert m.preemptions == 0
    assert m.context_switches == 0
    assert m.total_energy_mj == 0.0
    assert m.average_utilization == 0.0
    assert m.rejected_tasks == 0
    assert m.cluster_migrations == 0
    assert m.transitions == 0
    assert m.core_state_requests == 0
    assert m.frequency_requests == 0

    print("  SimulationMetrics struct: OK")


def test_response_time_stats_struct():
    """Test ResponseTimeStats struct fields."""
    s = sim.ResponseTimeStats()
    assert isinstance(s.min, float)
    assert isinstance(s.max, float)
    assert isinstance(s.mean, float)
    assert isinstance(s.median, float)
    assert isinstance(s.stddev, float)
    assert isinstance(s.percentile_95, float)
    assert isinstance(s.percentile_99, float)

    print("  ResponseTimeStats struct: OK")


def test_time_series_structs():
    """Test time-series interval structs."""
    fi = sim.FrequencyInterval()
    assert hasattr(fi, "start")
    assert hasattr(fi, "stop")
    assert hasattr(fi, "frequency")
    assert hasattr(fi, "cluster_id")

    ci = sim.CoreCountInterval()
    assert hasattr(ci, "start")
    assert hasattr(ci, "stop")
    assert hasattr(ci, "active_cores")
    assert hasattr(ci, "cluster_id")

    cfi = sim.ConfigInterval()
    assert hasattr(cfi, "start")
    assert hasattr(cfi, "stop")
    assert hasattr(cfi, "frequency")
    assert hasattr(cfi, "active_cores")

    print("  Time-series structs: OK")


def test_memory_writer_time_series():
    """Test time-series analysis via MemoryTraceWriter."""
    engine, proc, task = make_simple_engine()

    writer = sim.MemoryTraceWriter()
    engine.set_trace_writer(writer)

    scheduler = sim.EdfScheduler(engine, [proc])
    scheduler.add_server(task)
    allocator = sim.SingleSchedulerAllocator(engine, scheduler)
    engine.schedule_job_arrival(task, 0.0, 2.0)
    engine.run(5.0)

    # Call time-series functions (may return empty vectors for simple scenarios)
    freq_intervals = writer.track_frequency_changes()
    assert isinstance(freq_intervals, sim.FrequencyIntervalVector)

    core_intervals = writer.track_core_changes()
    assert isinstance(core_intervals, sim.CoreCountIntervalVector)

    config_intervals = writer.track_config_changes()
    assert isinstance(config_intervals, sim.ConfigIntervalVector)

    print("  MemoryTraceWriter time-series: OK")


def main():
    print("Running pyschedsim new API tests...")

    test_new_exports()
    test_add_task_with_id()
    test_null_trace_writer()
    test_memory_trace_writer()
    test_memory_trace_writer_compute_metrics()
    test_file_json_trace_writer()
    test_enable_grub()
    test_enable_cash()
    test_enable_dvfs_policies()
    test_enable_dpm()
    test_set_deadline_miss_policy()
    test_set_expected_arrivals()
    test_utilization_queries()
    test_cluster_class()
    test_simulation_metrics_struct()
    test_response_time_stats_struct()
    test_time_series_structs()
    test_memory_writer_time_series()

    print("\nAll pyschedsim new API tests passed!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
