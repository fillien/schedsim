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

TEST(FFCapAdaptiveLinearAllocatorTest, LowUtilizationUsesFullTarget) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Task: wcet=5, period=10 => util=0.5
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    FFCapAdaptiveLinearAllocator alloc(engine, plat.clusters_big_first());
    alloc.set_expected_total_util(2.0);

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(5.0));
    engine.run(time_from_seconds(0.5));

    // Below the fitted-model boundary, retain the full LITTLE-cluster target.
    EXPECT_DOUBLE_EQ(plat.little_cluster->u_target(), 1.0);
}

TEST(FFCapAdaptiveLinearAllocatorTest, ModelSetsUTargetAboveBoundary) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Task utilization is 0.2.
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine.platform().finalize();

    FFCapAdaptiveLinearAllocator alloc(engine, plat.clusters_big_first());
    alloc.set_expected_total_util(4.0);

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(2.0));
    engine.run(time_from_seconds(0.5));

    // 1.616*0.2 + 0.098*4.0 - 0.373 = 0.3422
    EXPECT_NEAR(plat.little_cluster->u_target(), 0.3422, 0.0001);
}

TEST(FFCapAdaptiveLinearAllocatorTest, ModelClampsTargetToOne) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0));
    engine.platform().finalize();

    FFCapAdaptiveLinearAllocator alloc(engine, plat.clusters_big_first());
    alloc.set_expected_total_util(4.0);

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(10.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_DOUBLE_EQ(plat.little_cluster->u_target(), 1.0);
}

// ============================================================
// FFCapAdaptivePolyAllocator
// ============================================================

TEST(FFCapAdaptivePolyAllocatorTest, ModelSetsUTarget) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine.platform().finalize();

    FFCapAdaptivePolyAllocator alloc(engine, plat.clusters_big_first());
    alloc.set_expected_total_util(5.0);

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(2.0));
    engine.run(time_from_seconds(0.5));

    // umax=0.2, U=5.0
    // C0 + C1*0.2 + C2*5.0 + C3*0.04 + C4*1.0 + C5*25.0
    // = -3.556483715 + 0.093087092 + 6.365803875 + (-0.112208653) + 0.651635123 + (-2.819627425)
    // ≈ 0.622206298
    EXPECT_NEAR(plat.little_cluster->u_target(), 0.622, 0.01);
}

TEST(FFCapAdaptivePolyAllocatorTest, KnownModelValues) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // umax≈0 and U=0 => raw result is C0 ≈ -3.556, clamped to 0.0
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(0.001));
    engine.platform().finalize();

    FFCapAdaptivePolyAllocator alloc(engine, plat.clusters_big_first());
    alloc.set_expected_total_util(0.0);

    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(0.001));
    engine.run(time_from_seconds(0.5));

    // C0 ≈ -3.556 => clamped to 0.0
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

// ============================================================
// Migration Tests
// ============================================================

TEST(MigrationTest, MigrationDisabledByDefault) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Small task: util=0.1, fits on little
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());
    // Do NOT call enable_migration()

    // First job: placed on little (FFCap sorts ascending by perf)
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(1.5));
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);

    // Second job: still on little (no migration)
    engine.schedule_job_arrival(task, time_from_seconds(10.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(11.5));
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);
}

TEST(MigrationTest, InactiveServerMigrates_FFCap) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Task: util=0.1 (wcet=1, period=10)
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());
    alloc.enable_migration();

    // First job: placed on little (FFCap sorts ascending by perf → little first)
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(1.5));
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);

    // Lower little's u_target so it rejects the task on re-evaluation
    plat.little_cluster->set_u_target(0.01);

    // Second job: server is Inactive, little rejects (scaled_util > u_target) → migrates to big
    engine.schedule_job_arrival(task, time_from_seconds(10.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(11.5));

    EXPECT_NE(plat.big_sched->find_server(task), nullptr);
}

TEST(MigrationTest, NoMigrationWhenRunning) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Task with long execution: util=0.5, wcet=5, period=10
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());
    alloc.enable_migration();

    // First job: placed on little
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(5.0));
    // Second job arrives while first is still Running (at t=3, before wcet=5 completes)
    engine.schedule_job_arrival(task, time_from_seconds(3.0), duration_from_seconds(5.0));
    engine.run(time_from_seconds(3.5));

    // Task should NOT have migrated — server was Running
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);
}

TEST(MigrationTest, RollbackOnRejection) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());
    alloc.enable_migration();

    // Place target on little
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(1.5));
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);

    // Set u_target=0 on BOTH clusters so all reject on re-evaluation
    plat.little_cluster->set_u_target(0.0);
    plat.big_cluster->set_u_target(0.0);

    // Second job: all clusters reject → rollback, stays on little
    engine.schedule_job_arrival(task, time_from_seconds(10.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(11.5));

    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);

    // Verify old cluster's accounting is intact after rollback
    double expected_scaled = plat.little_cluster->scaled_utilization(task_utilization(task));
    EXPECT_NEAR(plat.little_cluster->total_scaled_utilization(), expected_scaled, 1e-9);
}

TEST(MigrationTest, ZombieMigrationWithGrub) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);

    plat.little_sched->enable_grub();
    plat.big_sched->enable_grub();

    // Target task: wcet=1.0, period=10.0, util=0.1.
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    // Helper tasks placed via allocator to increase GRUB's scheduler_utilization_.
    // With a single server, M-GRUB bandwidth == U_i exactly → vt == now (strict > fails).
    // Helpers must be activated (receive jobs) so GRUB adds them to scheduler_utils_.
    // util=0.1 each → scaled on little = 0.2, fits GFB.
    std::vector<Task*> helpers;
    for (int i = 0; i < 3; ++i) {
        helpers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0),
            duration_from_seconds(1.0)));  // util=0.1 each
    }
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());
    alloc.enable_migration();

    // Place helpers on little first (long-running jobs to keep them active)
    for (auto* h : helpers) {
        engine.schedule_job_arrival(*h, time_from_seconds(0.0), h->wcet());
    }
    // Target job: short actual execution (0.1 ref units, wcet=1.0) → finishes early
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(0.1));
    // Run past job completion but before VT deadline
    // With 4 servers (total_u=0.4, u_max=0.1, m=4):
    //   bandwidth = max(1 - (4-3*0.1-0.4)/4, 0.01) = max(1-3.3/4, 0.01) = 0.175
    //   vt_increment = (0.175/0.1) * wall_elapsed. Job exec=0.1ref, speed=0.5, wall=0.2
    //   vt_increment = 1.75 * 0.2 = 0.35. VT timer at t=0.35. Job done at t≈0.2.
    engine.run(time_from_seconds(0.3));

    CbsServer* old_server = plat.little_sched->find_server(task);
    ASSERT_NE(old_server, nullptr);
    ASSERT_EQ(old_server->state(), CbsServer::State::NonContending);

    // Lower u_target to force migration
    plat.little_cluster->set_u_target(0.001);

    // Second job while NonContending → zombie migration to big
    engine.schedule_job_arrival(task, time_from_seconds(0.3), duration_from_seconds(0.1));
    engine.run(time_from_seconds(0.32));

    EXPECT_NE(plat.big_sched->find_server(task), nullptr);
    EXPECT_TRUE(old_server->is_pending_removal());

    // Run past VT deadline to clean up zombie
    engine.run(time_from_seconds(5.0));
    EXPECT_FALSE(old_server->is_pending_removal());
}

// Note: B2a (server==nullptr after M-GRUB detach) is unreachable in practice.
// M-GRUB's try_detach_server notifies Detached but does NOT erase task_to_server_,
// so find_server still returns non-null for detached servers. The nullptr guard
// in on_job_arrival is purely defensive.

TEST(MigrationTest, NoMigrationWhenReady) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);

    // 4 filler tasks to occupy all 4 little processors (util=0.1 each, scaled=0.2)
    std::vector<Task*> fillers;
    for (int i = 0; i < 4; ++i) {
        fillers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0),
            duration_from_seconds(1.0)));
    }
    // Target task: util=0.1
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());
    alloc.enable_migration();

    // Place fillers on little — they occupy all 4 processors
    for (auto* ft : fillers) {
        engine.schedule_job_arrival(*ft, time_from_seconds(0.0), ft->wcet());
    }
    // Place target on little too — server created, job queued (Ready, waiting for processor)
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // Target should be on little
    CbsServer* server = plat.little_sched->find_server(task);
    ASSERT_NE(server, nullptr);
    // Server should be Ready (queued, all procs busy) or Running
    EXPECT_TRUE(server->state() == CbsServer::State::Ready ||
                server->state() == CbsServer::State::Running);

    // Second job arrives while server is Ready/Running → should NOT migrate
    engine.schedule_job_arrival(task, time_from_seconds(1.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(1.5));

    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);
}

TEST(MigrationTest, InactiveServerMigrates_Adaptive) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Task: util=0.1
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    // Fillers to saturate little's scheduler: util=1.0 each × 4 = 4.0 (full capacity)
    std::vector<Task*> fillers;
    for (int i = 0; i < 4; ++i) {
        fillers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0),
            duration_from_seconds(10.0)));
    }
    engine.platform().finalize();

    FFCapAdaptiveLinearAllocator alloc(engine, plat.clusters_big_first());
    alloc.set_expected_total_util(1.0);  // low enough that model returns 1.0
    alloc.enable_migration();

    // First job: placed on little
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(1.5));
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);

    // Fill little's scheduler near capacity so can_admit fails on re-evaluation.
    // add_server_unchecked bypasses admission test and cluster-level tracking.
    for (auto* ft : fillers) {
        plat.little_sched->add_server_unchecked(*ft, ft->wcet(), ft->period());
    }

    // Second job: server Inactive, little's can_admit fails (scheduler near full) → migrates to big
    engine.schedule_job_arrival(task, time_from_seconds(10.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(11.5));

    EXPECT_NE(plat.big_sched->find_server(task), nullptr);
    // Verify old scheduler no longer has the server
    EXPECT_EQ(plat.little_sched->find_server(task), nullptr);

    // Verify cluster's total_scaled_utilization_ is 0 (Adaptive never uses try_admit_scaled)
    EXPECT_DOUBLE_EQ(plat.little_cluster->total_scaled_utilization(), 0.0);
    EXPECT_DOUBLE_EQ(plat.big_cluster->total_scaled_utilization(), 0.0);
}

TEST(MigrationTest, SameClusterSelected_NoMigration) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);
    // Task: util=0.1
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());
    alloc.enable_migration();

    // First job: placed on little
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(1.5));
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);

    // Don't change u_target — re-evaluation will select little again

    // Second job: same cluster selected → no migration
    engine.schedule_job_arrival(task, time_from_seconds(10.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(11.5));

    // Still on little, NOT on big
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);

    // Verify cluster accounting is correct (no double-count):
    // total_scaled_utilization should still reflect exactly one task
    double expected_scaled = plat.little_cluster->scaled_utilization(task_utilization(task));
    EXPECT_NEAR(plat.little_cluster->total_scaled_utilization(), expected_scaled, 1e-9);
}

TEST(MigrationTest, SchedulerRejectsAfterClusterAdmits_Rollback) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);

    // Target task: util=0.1
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    // 4 filler tasks to fill big's scheduler (created before finalize)
    std::vector<Task*> fillers;
    for (int i = 0; i < 4; ++i) {
        fillers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0),
            duration_from_seconds(10.0)));
    }
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());
    alloc.enable_migration();

    // Place target on little first
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(1.5));
    EXPECT_NE(plat.little_sched->find_server(task), nullptr);

    // Fill big's scheduler directly (bypass cluster-level tracking).
    // Big's scheduler now has total_utilization_ = 4.0, at capacity.
    for (auto* ft : fillers) {
        plat.big_sched->add_server_unchecked(*ft, ft->wcet(), ft->period());
    }

    // Lower little's u_target to force migration attempt toward big
    plat.little_cluster->set_u_target(0.001);

    // Second job: select_cluster picks big (cluster GFB passes since big's
    // total_scaled_utilization_ is 0), but can_admit pre-check fails (scheduler full).
    // Rollback: task stays on little.
    engine.schedule_job_arrival(task, time_from_seconds(10.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(11.5));

    EXPECT_NE(plat.little_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.big_sched->find_server(task), nullptr);

    // Verify new cluster's scaled utilization was properly rolled back (no phantom admission)
    EXPECT_DOUBLE_EQ(plat.big_cluster->total_scaled_utilization(), 0.0);
}

TEST(MigrationTest, NonContendingZombieMigration) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);

    plat.little_sched->enable_grub();
    plat.big_sched->enable_grub();

    // Target task: wcet=1.0, period=10.0, util=0.1.
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    // 3 helper tasks (util=0.1 each) to increase GRUB scheduler_utilization_.
    std::vector<Task*> helpers;
    for (int i = 0; i < 3; ++i) {
        helpers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0),
            duration_from_seconds(1.0)));
    }
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());
    alloc.enable_migration();

    // Activate helpers on little (long-running jobs)
    for (auto* h : helpers) {
        engine.schedule_job_arrival(*h, time_from_seconds(0.0), h->wcet());
    }
    // Target: short actual execution → finishes early → NonContending
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(0.1));
    engine.run(time_from_seconds(0.3));

    CbsServer* old_server = plat.little_sched->find_server(task);
    ASSERT_NE(old_server, nullptr);
    ASSERT_EQ(old_server->state(), CbsServer::State::NonContending);

    // Verify all servers' utilization counted: target(0.1) + 3 helpers(0.1) = 0.4
    EXPECT_NEAR(plat.little_sched->utilization(), 0.4, 1e-6);

    // Lower u_target to force migration
    plat.little_cluster->set_u_target(0.001);

    // Second job while NonContending → zombie migration to big
    engine.schedule_job_arrival(task, time_from_seconds(0.3), duration_from_seconds(0.1));
    engine.run(time_from_seconds(0.32));

    // Task should now be on big
    EXPECT_NE(plat.big_sched->find_server(task), nullptr);

    // Old server should be zombie
    EXPECT_TRUE(old_server->is_pending_removal());

    // Zombie's utilization must still be counted in old scheduler (GRUB correctness)
    EXPECT_NEAR(plat.little_sched->utilization(), 0.4, 1e-6);

    // task_to_server mapping should be cleared on old scheduler
    EXPECT_EQ(plat.little_sched->find_server(task), nullptr);

    // Run past VT deadline to trigger zombie cleanup
    engine.run(time_from_seconds(5.0));

    // Zombie cleaned up: pending_removal cleared, utilization decremented
    EXPECT_FALSE(old_server->is_pending_removal());
    // Only helpers' utilization remains
    EXPECT_NEAR(plat.little_sched->utilization(), 0.3, 1e-6);
}

TEST(MigrationTest, ExpectedArrivalsTransferred) {
    Engine engine;
    auto plat = BigLittlePlatform::create(engine);

    plat.little_sched->enable_grub();
    plat.big_sched->enable_grub();

    // Task: util=0.05
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(0.5));
    engine.platform().finalize();

    FFCapAllocator alloc(engine, plat.clusters_big_first());
    alloc.enable_migration();

    // Set expected arrivals on old scheduler
    plat.little_sched->set_expected_arrivals(task, 5);

    // Send 2 jobs to little
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(0.5));
    engine.run(time_from_seconds(1.5));
    engine.schedule_job_arrival(task, time_from_seconds(10.0), duration_from_seconds(0.5));
    engine.run(time_from_seconds(11.5));

    // Verify 2 arrivals counted on old scheduler
    EXPECT_EQ(plat.little_sched->get_arrival_count(task), 2u);

    // Lower u_target to force migration on 3rd job
    plat.little_cluster->set_u_target(0.001);

    engine.schedule_job_arrival(task, time_from_seconds(20.0), duration_from_seconds(0.5));
    engine.run(time_from_seconds(21.5));

    // Task should have migrated to big
    EXPECT_NE(plat.big_sched->find_server(task), nullptr);

    // Expected arrivals should be transferred: 5 - 2 = 3 remaining
    auto transferred = plat.big_sched->get_expected_arrivals(task);
    EXPECT_TRUE(transferred.has_value());
    if (transferred) {
        EXPECT_EQ(*transferred, 3u);
    }
}
