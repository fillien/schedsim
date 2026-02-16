#include <schedsim/algo/cluster.hpp>
#include <schedsim/algo/counting_allocator.hpp>
#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/ff_big_first_allocator.hpp>
#include <schedsim/algo/ff_cap_adaptive_linear_allocator.hpp>
#include <schedsim/algo/ff_cap_adaptive_poly_allocator.hpp>
#include <schedsim/algo/ff_cap_allocator.hpp>
#include <schedsim/algo/ff_lb_allocator.hpp>
#include <schedsim/algo/ff_little_first_allocator.hpp>
#include <schedsim/algo/mcts_allocator.hpp>
#include <schedsim/algo/task_utils.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace schedsim::algo;
using namespace schedsim::core;

// Helper: build a big.LITTLE platform on a given engine and return two clusters.
// big:    4 procs, perf=2.0, freq_max=2000
// little: 4 procs, perf=1.0, freq_max=1000
struct BigLittlePlatform {
    std::unique_ptr<EdfScheduler> big_sched;
    std::unique_ptr<EdfScheduler> little_sched;
    std::unique_ptr<Cluster> big_cluster;
    std::unique_ptr<Cluster> little_cluster;

    static BigLittlePlatform create(Engine& engine) {
        auto& big_type = engine.platform().add_processor_type("big", 2.0);
        auto& little_type = engine.platform().add_processor_type("little", 1.0);

        auto& big_cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
        auto& little_cd = engine.platform().add_clock_domain(Frequency{200.0}, Frequency{1000.0});

        auto& pd = engine.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });

        std::vector<Processor*> big_procs;
        std::vector<Processor*> little_procs;
        for (int i = 0; i < 4; ++i) {
            big_procs.push_back(&engine.platform().add_processor(big_type, big_cd, pd));
        }
        for (int i = 0; i < 4; ++i) {
            little_procs.push_back(&engine.platform().add_processor(little_type, little_cd, pd));
        }

        BigLittlePlatform plat;
        plat.big_sched = std::make_unique<EdfScheduler>(engine, big_procs);
        plat.little_sched = std::make_unique<EdfScheduler>(engine, little_procs);
        // reference_freq_max = big's freq_max = 2000
        plat.big_cluster = std::make_unique<Cluster>(big_cd, *plat.big_sched, 2.0, 2000.0);
        plat.little_cluster = std::make_unique<Cluster>(little_cd, *plat.little_sched, 1.0, 2000.0);
        return plat;
    }

    std::vector<Cluster*> clusters_big_first() {
        return {big_cluster.get(), little_cluster.get()};
    }
    std::vector<Cluster*> clusters_little_first() {
        return {little_cluster.get(), big_cluster.get()};
    }
};

// ============================================================
// FFBigFirstAllocator
// ============================================================

TEST(FFBigFirstAllocatorTest, PrefersBigCluster) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Task: wcet=1, period=10 => util=0.1
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFBigFirstAllocator alloc(engine, plat.clusters_big_first());

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // Task should be on big cluster (server created there)
    EXPECT_NE(plat.big_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.little_sched->find_server(task), nullptr);
}

TEST(FFBigFirstAllocatorTest, FallsBackToLittleWhenBigFull) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);

    // Fill big cluster: 4 procs, 4 tasks with util=1.0 each
    std::vector<Task*> filler_tasks;
    for (int i = 0; i < 4; ++i) {
        filler_tasks.push_back(
            &engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    // Target task: small enough for little
    auto& target = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFBigFirstAllocator alloc(engine, plat.clusters_big_first());

    TimePoint t = time_from_seconds(0.0);
    for (auto* ft : filler_tasks) {
        engine.schedule_job_arrival(*ft, t, ft->wcet());
    }
    engine.schedule_job_arrival(target, t, duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // target should have fallen back to little
    EXPECT_EQ(plat.big_sched->find_server(target), nullptr);
    EXPECT_NE(plat.little_sched->find_server(target), nullptr);
}

TEST(FFBigFirstAllocatorTest, ReturnsNullWhenBothFull) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);

    // Fill both clusters: 4+4 tasks with util=1.0
    std::vector<Task*> filler_tasks;
    for (int i = 0; i < 8; ++i) {
        filler_tasks.push_back(
            &engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    auto& target = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFBigFirstAllocator alloc(engine, plat.clusters_big_first());

    TimePoint t = time_from_seconds(0.0);
    for (auto* ft : filler_tasks) {
        engine.schedule_job_arrival(*ft, t, ft->wcet());
    }
    engine.schedule_job_arrival(target, t, duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // target should be rejected (no server anywhere)
    EXPECT_EQ(plat.big_sched->find_server(target), nullptr);
    EXPECT_EQ(plat.little_sched->find_server(target), nullptr);
}

// ============================================================
// FFLittleFirstAllocator
// ============================================================

TEST(FFLittleFirstAllocatorTest, PrefersLittleCluster) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFLittleFirstAllocator alloc(engine, plat.clusters_big_first());

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
}

TEST(FFLittleFirstAllocatorTest, FallsBackToBig) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);

    // Fill little (4 procs)
    std::vector<Task*> fillers;
    for (int i = 0; i < 4; ++i) {
        fillers.push_back(
            &engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    auto& target = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFLittleFirstAllocator alloc(engine, plat.clusters_big_first());

    TimePoint t = time_from_seconds(0.0);
    for (auto* ft : fillers) {
        engine.schedule_job_arrival(*ft, t, ft->wcet());
    }
    engine.schedule_job_arrival(target, t, duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_NE(plat.big_sched->find_server(target), nullptr);
}

TEST(FFLittleFirstAllocatorTest, CounterIncrements) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& t1 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    auto& t2 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFLittleFirstAllocator alloc(engine, plat.clusters_big_first());
    EXPECT_EQ(alloc.allocation_count(), 0u);

    engine.schedule_job_arrival(t1, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.schedule_job_arrival(t2, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_EQ(alloc.allocation_count(), 2u);
}

// ============================================================
// CountingAllocator
// ============================================================

TEST(CountingAllocatorTest, NaturalOrderPlacement) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    // Natural order: big first
    CountingAllocator alloc(engine, plat.clusters_big_first());

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // First cluster in natural order is big
    EXPECT_NE(plat.big_sched->find_server(task), nullptr);
}

TEST(CountingAllocatorTest, CounterIncrements) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& t1 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    auto& t2 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    CountingAllocator alloc(engine, plat.clusters_big_first());
    EXPECT_EQ(alloc.allocation_count(), 0u);

    engine.schedule_job_arrival(t1, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.schedule_job_arrival(t2, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_EQ(alloc.allocation_count(), 2u);
}

// ============================================================
// FFCapAllocator
// ============================================================

TEST(FFCapAllocatorTest, RespectsUTarget) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Task: wcet=5, period=10 => util=0.5
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    // Set little u_target very low so it rejects the task on capacity grounds
    plat.little_cluster->set_u_target(0.01);

    FFCapAllocator alloc(engine, plat.clusters_big_first());

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(5.0));
    engine.run(time_from_seconds(0.5));

    // Little should be skipped (scaled_util > u_target), big should accept
    EXPECT_NE(plat.big_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.little_sched->find_server(task), nullptr);
}

TEST(FFCapAllocatorTest, PrefersLittleWhenCapacityAllows) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Small task: wcet=0.1, period=10 => util=0.01
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(0.1));
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(0.1));
    engine.run(time_from_seconds(0.5));

    // FFCap sorts ascending by perf, so little is tried first
    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
}

// ============================================================
// FFCapAdaptiveLinearAllocator
// ============================================================

TEST(FFCapAdaptiveLinearAllocatorTest, ModelSetsUTarget) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Task: wcet=5, period=10 => util=0.5
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    FFCapAdaptiveLinearAllocator alloc(engine, plat.clusters_big_first());
    alloc.set_expected_total_util(2.0);

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(5.0));
    engine.run(time_from_seconds(0.5));

    // Model: A*umax + B*U + C = 1.616*0.5 + 0.098*2.0 + (-0.373) = 0.808 + 0.196 - 0.373 = 0.631
    // This target is set on the little cluster (smallest perf, sorted first)
    EXPECT_NEAR(plat.little_cluster->u_target(), 0.631, 0.01);
}

TEST(FFCapAdaptiveLinearAllocatorTest, KnownModelValues) {
    // Verify the model output for known inputs
    // umax=1.0, U=4.0 => 1.616*1.0 + 0.098*4.0 + (-0.373) = 1.616+0.392-0.373 = 1.635 => clamped to 1.0
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0));
    engine.platform().finalize();

    FFCapAdaptiveLinearAllocator alloc(engine, plat.clusters_big_first());
    alloc.set_expected_total_util(4.0);

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(10.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_DOUBLE_EQ(plat.little_cluster->u_target(), 1.0);  // clamped
}

// ============================================================
// FFCapAdaptivePolyAllocator
// ============================================================

TEST(FFCapAdaptivePolyAllocatorTest, ModelSetsUTarget) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    FFCapAdaptivePolyAllocator alloc(engine, plat.clusters_big_first());
    alloc.set_expected_total_util(2.0);

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(5.0));
    engine.run(time_from_seconds(0.5));

    // umax=0.5, U=2.0
    // C0 + C1*0.5 + C2*2.0 + C3*0.25 + C4*1.0 + C5*4.0
    // = -0.285854319 + 1.169853995 + 0.063796954 + (-0.344100337) + (-0.037369647) + 0.030530928
    // ≈ 0.596857574
    EXPECT_NEAR(plat.little_cluster->u_target(), 0.597, 0.01);
}

TEST(FFCapAdaptivePolyAllocatorTest, KnownModelValues) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // umax=0.0 => all terms with umax vanish
    // target = C0 + C2*U + C5*U^2
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(0.001));
    engine.platform().finalize();

    FFCapAdaptivePolyAllocator alloc(engine, plat.clusters_big_first());
    alloc.set_expected_total_util(0.0);

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(0.001));
    engine.run(time_from_seconds(0.5));

    // C0 ≈ -0.286 => clamped to 0.0
    EXPECT_DOUBLE_EQ(plat.little_cluster->u_target(), 0.0);
}

// ============================================================
// FFLbAllocator
// ============================================================

TEST(FFLbAllocatorTest, SetsLittleUTargetFromBigUtilization) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);

    // filler: wcet=5, period=10 => util=0.5
    // On first call, big util=0 => avg_big=0 => little u_target=0, so filler goes to big.
    // On second call, big util=0.5 => avg_big=0.5/4=0.125 => little u_target=0.125*1.0=0.125.
    auto& filler = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    auto& target = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(0.1));
    engine.platform().finalize();

    FFLbAllocator alloc(engine, plat.clusters_big_first());

    engine.schedule_job_arrival(filler, time_from_seconds(0.0), duration_from_seconds(5.0));
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(0.1));
    engine.run(time_from_seconds(0.5));

    // Both tasks should have been placed
    std::size_t total_servers =
        plat.big_sched->server_count() + plat.little_sched->server_count();
    EXPECT_EQ(total_servers, 2u);

    // Verify the dynamically computed u_target on little
    EXPECT_DOUBLE_EQ(plat.little_cluster->u_target(), 0.125);
}

TEST(FFLbAllocatorTest, ZeroBigUtil_SendsToBig) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Very small task — but big has 0 utilization so avg_big=0, little u_target=0
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(0.1));
    engine.platform().finalize();

    FFLbAllocator alloc(engine, plat.clusters_big_first());

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(0.1));
    engine.run(time_from_seconds(0.5));

    // Little u_target = 0 (no big load), so task goes to big (u_target=1.0 default)
    EXPECT_NE(plat.big_sched->find_server(task), nullptr);
}

// ============================================================
// MCTSAllocator
// ============================================================

TEST(MCTSAllocatorTest, FollowsPattern) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& t1 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    auto& t2 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    // Pattern: [1, 0] => first task to cluster[1] (little), second to cluster[0] (big)
    MCTSAllocator alloc(engine, plat.clusters_big_first(), {1, 0});

    engine.schedule_job_arrival(t1, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.schedule_job_arrival(t2, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_NE(plat.little_sched->find_server(t1), nullptr);
    EXPECT_NE(plat.big_sched->find_server(t2), nullptr);
}

TEST(MCTSAllocatorTest, RandomAfterPatternExhaustion) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& t1 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    auto& t2 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    // Pattern: [0] => only one entry, second task gets random
    MCTSAllocator alloc(engine, plat.clusters_big_first(), {0});

    engine.schedule_job_arrival(t1, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.schedule_job_arrival(t2, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // First task should definitely be on big (index 0)
    EXPECT_NE(plat.big_sched->find_server(t1), nullptr);
    // Second task goes somewhere (random) — just verify it was placed
    bool placed = plat.big_sched->find_server(t2) != nullptr ||
                  plat.little_sched->find_server(t2) != nullptr;
    EXPECT_TRUE(placed);
}

TEST(MCTSAllocatorTest, CounterIncrements) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& t1 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    MCTSAllocator alloc(engine, plat.clusters_big_first(), {0});
    EXPECT_EQ(alloc.allocation_count(), 0u);

    engine.schedule_job_arrival(t1, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_EQ(alloc.allocation_count(), 1u);
}

TEST(MCTSAllocatorTest, NoAdmissionRejection) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);

    // 9 tasks with util=1.0 each — exceeds total capacity of 8 procs
    std::vector<Task*> tasks;
    for (int i = 0; i < 9; ++i) {
        tasks.push_back(
            &engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    engine.platform().finalize();

    // Pattern puts all on cluster 0 (big, 4 procs) — will overflow
    std::vector<unsigned> pat;
    for (int i = 0; i < 9; ++i) {
        pat.push_back(0);
    }
    MCTSAllocator alloc(engine, plat.clusters_big_first(), pat);

    TimePoint t = time_from_seconds(0.0);
    for (auto* task : tasks) {
        engine.schedule_job_arrival(*task, t, task->wcet());
    }

    // Should not throw — AdmissionError is caught by MultiClusterAllocator
    EXPECT_NO_THROW(engine.run(time_from_seconds(0.5)));
}

// ============================================================
// task_utilization utility
// ============================================================

TEST(TaskUtilsTest, BasicComputation) {
    Engine engine;
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(3.0));
    engine.platform().finalize();
    EXPECT_DOUBLE_EQ(task_utilization(task), 0.3);
}
