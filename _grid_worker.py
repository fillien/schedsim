"""Worker function for grid search parallelization."""
import os
import sys

_ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_ROOT, "build", "python"))
os.chdir(_ROOT)

import pyschedsim as sim

PLATFORM_FULL = "platforms/orion6.json"


def run_grid(args):
    taskset, u_target_small, u_target_medium = args

    engine = sim.Engine()
    sim.load_platform(engine, PLATFORM_FULL)
    scenario = sim.load_scenario(taskset["path"])

    tasks = sim.inject_scenario(engine, scenario)
    for i, task in enumerate(tasks):
        sim.schedule_arrivals(engine, task, scenario.tasks[i].jobs)
    engine.platform.finalize()

    platform = engine.platform
    ref_freq_max = max(
        platform.clock_domain(i).freq_max
        for i in range(platform.clock_domain_count)
    )

    clusters = []
    for i in range(platform.clock_domain_count):
        cd = platform.clock_domain(i)
        procs = cd.get_processors()
        if not procs:
            continue
        sched = sim.EdfScheduler(engine, procs)
        sched.enable_grub()
        sched.set_admission_test(sim.AdmissionTest.GFB)
        perf = procs[0].type().performance
        cluster = sim.Cluster(cd, sched, perf, ref_freq_max)

        if perf < 1.0:
            cluster.set_u_target(u_target_small)
        elif cd.freq_max < ref_freq_max:
            cluster.set_u_target(u_target_medium)
        clusters.append(cluster)

    _alloc = sim.FFCapAllocator(engine, clusters)

    writer = sim.MemoryTraceWriter()
    engine.set_trace_writer(writer)
    engine.run()

    metrics = writer.compute_metrics()
    total_arrivals = metrics.total_jobs + metrics.rejected_arrivals

    return {
        "total_util": taskset["total_util"],
        "seed": taskset["seed"],
        "u_max": taskset["u_max"],
        "u_target_small": round(u_target_small, 4),
        "u_target_medium": round(u_target_medium, 4),
        "total_jobs": metrics.total_jobs,
        "rejected_arrivals": metrics.rejected_arrivals,
        "total_arrivals": total_arrivals,
    }
