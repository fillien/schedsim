#include <schedsim/algo/best_fit_allocator.hpp>
#include <schedsim/algo/cluster.hpp>
#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/ff_big_first_allocator.hpp>
#include <schedsim/algo/first_fit_allocator.hpp>
#include <schedsim/algo/task_utils.hpp>
#include <schedsim/algo/worst_fit_allocator.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <schedsim/io/trace_writers.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace schedsim::algo;
using namespace schedsim::core;

// ============================================================
// Test platform helpers
// ============================================================

// Uniform platform: 4 identical processors, 1 clock domain
struct UniformPerCorePlatform {
    std::vector<std::unique_ptr<EdfScheduler>> schedulers;
    std::vector<std::unique_ptr<Cluster>> clusters;
    std::vector<Cluster*> cluster_ptrs;
    std::vector<Processor*> procs;

    static UniformPerCorePlatform create(Engine& engine, int num_procs = 4) {
        auto& pt = engine.platform().add_processor_type("cpu", 1.0);
        auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
        auto& pd = engine.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });

        UniformPerCorePlatform plat;
        for (int i = 0; i < num_procs; ++i) {
            plat.procs.push_back(&engine.platform().add_processor(pt, cd, pd));
        }

        double ref_freq_max = cd.freq_max().mhz;
        for (auto* proc : plat.procs) {
            auto sched = std::make_unique<EdfScheduler>(engine,
                std::vector<Processor*>{proc});
            auto cluster = std::make_unique<Cluster>(
                proc->clock_domain(), *sched, proc->type().performance(), ref_freq_max);
            cluster->set_processor_id(proc->id());
            plat.cluster_ptrs.push_back(cluster.get());
            plat.schedulers.push_back(std::move(sched));
            plat.clusters.push_back(std::move(cluster));
        }
        return plat;
    }
};

// Heterogeneous big.LITTLE: 4 big (perf=2.0) + 4 little (perf=1.0)
struct HeterogeneousPerCorePlatform {
    std::vector<std::unique_ptr<EdfScheduler>> schedulers;
    std::vector<std::unique_ptr<Cluster>> clusters;
    std::vector<Cluster*> cluster_ptrs;  // [big0..big3, little0..little3]

    static HeterogeneousPerCorePlatform create(Engine& engine) {
        auto& big_type = engine.platform().add_processor_type("big", 2.0);
        auto& little_type = engine.platform().add_processor_type("little", 1.0);

        auto& big_cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
        auto& little_cd = engine.platform().add_clock_domain(Frequency{200.0}, Frequency{1000.0});

        auto& pd = engine.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });

        double ref_freq_max = 2000.0;

        HeterogeneousPerCorePlatform plat;

        // Big cores first
        for (int i = 0; i < 4; ++i) {
            auto& proc = engine.platform().add_processor(big_type, big_cd, pd);
            auto sched = std::make_unique<EdfScheduler>(engine,
                std::vector<Processor*>{&proc});
            auto cluster = std::make_unique<Cluster>(
                proc.clock_domain(), *sched, proc.type().performance(), ref_freq_max);
            cluster->set_processor_id(proc.id());
            plat.cluster_ptrs.push_back(cluster.get());
            plat.schedulers.push_back(std::move(sched));
            plat.clusters.push_back(std::move(cluster));
        }

        // Little cores
        for (int i = 0; i < 4; ++i) {
            auto& proc = engine.platform().add_processor(little_type, little_cd, pd);
            auto sched = std::make_unique<EdfScheduler>(engine,
                std::vector<Processor*>{&proc});
            auto cluster = std::make_unique<Cluster>(
                proc.clock_domain(), *sched, proc.type().performance(), ref_freq_max);
            cluster->set_processor_id(proc.id());
            plat.cluster_ptrs.push_back(cluster.get());
            plat.schedulers.push_back(std::move(sched));
            plat.clusters.push_back(std::move(cluster));
        }

        return plat;
    }
};

// Per-cluster big.LITTLE (for comparison tests)
struct BigLittlePerClusterPlatform {
    std::unique_ptr<EdfScheduler> big_sched;
    std::unique_ptr<EdfScheduler> little_sched;
    std::unique_ptr<Cluster> big_cluster;
    std::unique_ptr<Cluster> little_cluster;

    static BigLittlePerClusterPlatform create(Engine& engine) {
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

        BigLittlePerClusterPlatform plat;
        plat.big_sched = std::make_unique<EdfScheduler>(engine, big_procs);
        plat.little_sched = std::make_unique<EdfScheduler>(engine, little_procs);
        plat.big_cluster = std::make_unique<Cluster>(big_cd, *plat.big_sched, 2.0, 2000.0);
        plat.little_cluster = std::make_unique<Cluster>(little_cd, *plat.little_sched, 1.0, 2000.0);
        return plat;
    }

    std::vector<Cluster*> clusters_big_first() {
        return {big_cluster.get(), little_cluster.get()};
    }
};

// ============================================================
// FirstFitAllocator — per-core uniform
// ============================================================

TEST(PartitionedEdfTest, FF_PerCore_PlacesOnFirstCore) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(5.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_NE(plat.schedulers[0]->find_server(task), nullptr);
    EXPECT_EQ(plat.schedulers[1]->find_server(task), nullptr);
    EXPECT_EQ(plat.schedulers[2]->find_server(task), nullptr);
    EXPECT_EQ(plat.schedulers[3]->find_server(task), nullptr);
}

TEST(PartitionedEdfTest, FF_PerCore_SkipsFullCore) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    // Filler: U=1.0 fills core 0
    auto& filler = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0));
    // Target: U=0.5
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(filler, time_from_seconds(0.0), duration_from_seconds(10.0));
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(5.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_NE(plat.schedulers[0]->find_server(filler), nullptr);
    EXPECT_NE(plat.schedulers[1]->find_server(target), nullptr);
}

TEST(PartitionedEdfTest, FF_PerCore_FillsSequentially) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    std::vector<Task*> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(8.0)));
    }
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    for (auto* t : tasks) {
        engine.schedule_job_arrival(*t, time_from_seconds(0.0), t->wcet());
    }
    engine.run(time_from_seconds(0.5));

    for (int i = 0; i < 4; ++i) {
        EXPECT_NE(plat.schedulers[i]->find_server(*tasks[i]), nullptr)
            << "Task " << i << " should be on scheduler " << i;
    }
}

TEST(PartitionedEdfTest, FF_PerCore_RejectsWhenAllFull) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    std::vector<Task*> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    auto& extra = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    for (auto* t : tasks) {
        engine.schedule_job_arrival(*t, time_from_seconds(0.0), t->wcet());
    }
    engine.schedule_job_arrival(extra, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // Extra should be rejected (no server on any scheduler)
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(plat.schedulers[i]->find_server(extra), nullptr);
    }
}

TEST(PartitionedEdfTest, FF_PerCore_RejectsOverUnitUtil) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine, 1);
    // U = 11/10 = 1.1 > 1.0
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(11.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(11.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_EQ(plat.schedulers[0]->find_server(task), nullptr);
}

// ============================================================
// FirstFitAllocator — per-core heterogeneous
// ============================================================

TEST(PartitionedEdfTest, FF_PerCore_Hetero_BigFirst) {
    Engine engine;
    auto plat = HeterogeneousPerCorePlatform::create(engine);
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(3.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(3.0));
    engine.run(time_from_seconds(0.5));

    // Big cores come first in iteration order
    EXPECT_NE(plat.schedulers[0]->find_server(task), nullptr);
}

TEST(PartitionedEdfTest, FF_PerCore_Hetero_ScaledUtilRejectsLittle) {
    Engine engine;
    auto plat = HeterogeneousPerCorePlatform::create(engine);
    // Task U=0.6. On little: scaled = 0.6 * (2000/1000) / 1.0 = 1.2 > 1.0 (u_target)
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(6.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(6.0));
    engine.run(time_from_seconds(0.5));

    // Must be on a big core (little scaled util > 1.0)
    EXPECT_NE(plat.schedulers[0]->find_server(task), nullptr);
    for (int i = 4; i < 8; ++i) {
        EXPECT_EQ(plat.schedulers[i]->find_server(task), nullptr);
    }
}

TEST(PartitionedEdfTest, FF_PerCore_Hetero_FillBigSpillToLittle) {
    Engine engine;
    auto plat = HeterogeneousPerCorePlatform::create(engine);
    // Fill 4 big cores with U=1.0
    std::vector<Task*> fillers;
    for (int i = 0; i < 4; ++i) {
        fillers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    // Small task U=0.3 that fits on little
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(3.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    for (auto* f : fillers) {
        engine.schedule_job_arrival(*f, time_from_seconds(0.0), f->wcet());
    }
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(3.0));
    engine.run(time_from_seconds(0.5));

    // Target should be on first little core (index 4)
    EXPECT_NE(plat.schedulers[4]->find_server(target), nullptr);
}

// ============================================================
// FirstFitAllocator — per-cluster
// ============================================================

TEST(PartitionedEdfTest, FF_PerCluster_NaturalOrder) {
    Engine engine;
    auto plat = BigLittlePerClusterPlatform::create(engine);
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.clusters_big_first());
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_NE(plat.big_sched->find_server(task), nullptr);
    EXPECT_EQ(plat.little_sched->find_server(task), nullptr);
}

TEST(PartitionedEdfTest, FF_PerCluster_FallbackToSecond) {
    Engine engine;
    auto plat = BigLittlePerClusterPlatform::create(engine);
    // Fill big cluster (4 procs)
    std::vector<Task*> fillers;
    for (int i = 0; i < 4; ++i) {
        fillers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.clusters_big_first());
    for (auto* f : fillers) {
        engine.schedule_job_arrival(*f, time_from_seconds(0.0), f->wcet());
    }
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_EQ(plat.big_sched->find_server(target), nullptr);
    EXPECT_NE(plat.little_sched->find_server(target), nullptr);
}

// ============================================================
// WorstFitAllocator — per-core
// ============================================================

TEST(PartitionedEdfTest, WF_PerCore_SpreadsTasks) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    std::vector<Task*> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0)));
    }
    engine.platform().finalize();

    WorstFitAllocator alloc(engine, plat.cluster_ptrs);
    for (auto* t : tasks) {
        engine.schedule_job_arrival(*t, time_from_seconds(0.0), t->wcet());
    }
    engine.run(time_from_seconds(0.5));

    // Each task on a different core (worst-fit spreads evenly)
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(plat.schedulers[i]->server_count(), 1u)
            << "Scheduler " << i << " should have exactly 1 server";
    }
}

TEST(PartitionedEdfTest, WF_PerCore_PicksEmptiest) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    // Pre-fill: core 0 U=0.8, core 1 U=0.2, core 2 U=0.5, core 3 empty
    auto& t0 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(8.0));
    auto& t1 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    auto& t2 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    // Target task U=0.1
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    // First, use FF to place t0, t1, t2 on specific cores
    // We can pre-place by adding servers directly
    plat.schedulers[0]->add_server(t0);
    plat.schedulers[1]->add_server(t1);
    plat.schedulers[2]->add_server(t2);

    WorstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // Core 3 is emptiest (remaining=1.0)
    EXPECT_NE(plat.schedulers[3]->find_server(target), nullptr);
}

TEST(PartitionedEdfTest, WF_PerCore_TieBreaksFirstInOrder) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    WorstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // All cores equally empty — first in order wins
    EXPECT_NE(plat.schedulers[0]->find_server(task), nullptr);
}

TEST(PartitionedEdfTest, WF_PerCore_RejectsWhenAllFull) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    std::vector<Task*> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    auto& extra = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    WorstFitAllocator alloc(engine, plat.cluster_ptrs);
    for (auto* t : tasks) {
        engine.schedule_job_arrival(*t, time_from_seconds(0.0), t->wcet());
    }
    engine.schedule_job_arrival(extra, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(plat.schedulers[i]->find_server(extra), nullptr);
    }
}

// ============================================================
// WorstFitAllocator — per-core heterogeneous
// ============================================================

TEST(PartitionedEdfTest, WF_PerCore_Hetero_PicksEmptiest) {
    Engine engine;
    auto plat = HeterogeneousPerCorePlatform::create(engine);
    // Pre-fill: big[0] at U=0.8, little[0] at U=0.1
    auto& tb = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(8.0));
    auto& tl = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    // Target: U=0.1 (fits on both big and little)
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    plat.schedulers[0]->add_server(tb);   // big[0] remaining=0.2
    plat.schedulers[4]->add_server(tl);   // little[0] remaining=0.9

    WorstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // All other cores are empty (remaining=1.0), WF picks big[1] (first empty, highest remaining)
    EXPECT_NE(plat.schedulers[1]->find_server(target), nullptr);
}

TEST(PartitionedEdfTest, WF_PerCore_Hetero_SkipsScaledUtilViolation) {
    Engine engine;
    auto plat = HeterogeneousPerCorePlatform::create(engine);
    // Fill all big cores
    std::vector<Task*> fillers;
    for (int i = 0; i < 4; ++i) {
        fillers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    // Task with U=0.6: scaled_util on little = 0.6 * (2000/1000) / 1.0 = 1.2 > 1.0
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(6.0));
    engine.platform().finalize();

    for (int i = 0; i < 4; ++i) {
        plat.schedulers[i]->add_server(*fillers[i]);
    }

    WorstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(6.0));
    engine.run(time_from_seconds(0.5));

    // Big cores full, little cores have high remaining but scaled_util rejects → task rejected
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(plat.schedulers[i]->find_server(target), nullptr);
    }
}

// ============================================================
// WorstFitAllocator — per-cluster
// ============================================================

TEST(PartitionedEdfTest, WF_PerCluster_PicksMostFree) {
    Engine engine;
    auto plat = BigLittlePerClusterPlatform::create(engine);
    // Pre-fill big with U=3.5 (remain=0.5), little with U=1.0 (remain=3.0)
    // Big: 7 tasks U=0.5 each
    std::vector<Task*> big_fillers;
    for (int i = 0; i < 7; ++i) {
        big_fillers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0)));
    }
    // Little: 2 tasks U=0.5 each = 1.0
    std::vector<Task*> little_fillers;
    for (int i = 0; i < 2; ++i) {
        little_fillers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0)));
    }
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    // Pre-place servers
    for (auto* f : big_fillers) {
        plat.big_sched->add_server(*f);
    }
    for (auto* f : little_fillers) {
        plat.little_sched->add_server(*f);
    }

    WorstFitAllocator alloc(engine, plat.clusters_big_first());
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // Little has more remaining capacity (3.0 > 0.5)
    EXPECT_EQ(plat.big_sched->find_server(target), nullptr);
    EXPECT_NE(plat.little_sched->find_server(target), nullptr);
}

// ============================================================
// BestFitAllocator — per-core
// ============================================================

TEST(PartitionedEdfTest, BF_PerCore_PacksTightly) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine, 3);
    // Core 0: U=0.6 (remain=0.4), Core 1: U=0.3 (remain=0.7), Core 2: empty (remain=1.0)
    auto& t0 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(6.0));
    auto& t1 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(3.0));
    // Target U=0.3
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(3.0));
    engine.platform().finalize();

    plat.schedulers[0]->add_server(t0);
    plat.schedulers[1]->add_server(t1);

    BestFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(3.0));
    engine.run(time_from_seconds(0.5));

    // Core 0 has tightest fit (remaining 0.4, can admit 0.3)
    EXPECT_NE(plat.schedulers[0]->find_server(target), nullptr);
}

TEST(PartitionedEdfTest, BF_PerCore_SkipsCoreThatCantAdmit) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine, 2);
    // Core 0: U=0.9, Core 1: U=0.5
    auto& t0 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(9.0));
    auto& t1 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    // Target U=0.2
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine.platform().finalize();

    plat.schedulers[0]->add_server(t0);
    plat.schedulers[1]->add_server(t1);

    BestFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(2.0));
    engine.run(time_from_seconds(0.5));

    // Core 0 can't admit (0.9+0.2=1.1>1.0), goes to core 1
    EXPECT_EQ(plat.schedulers[0]->find_server(target), nullptr);
    EXPECT_NE(plat.schedulers[1]->find_server(target), nullptr);
}

TEST(PartitionedEdfTest, BF_PerCore_TieBreaksFirstInOrder) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine, 2);
    // Both cores at U=0.5
    auto& t0 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    auto& t1 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    plat.schedulers[0]->add_server(t0);
    plat.schedulers[1]->add_server(t1);

    BestFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // Tied remaining=0.5 — first in order wins
    EXPECT_NE(plat.schedulers[0]->find_server(target), nullptr);
}

TEST(PartitionedEdfTest, BF_PerCore_RejectsWhenAllFull) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    std::vector<Task*> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    auto& extra = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    BestFitAllocator alloc(engine, plat.cluster_ptrs);
    for (auto* t : tasks) {
        engine.schedule_job_arrival(*t, time_from_seconds(0.0), t->wcet());
    }
    engine.schedule_job_arrival(extra, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(plat.schedulers[i]->find_server(extra), nullptr);
    }
}

// ============================================================
// BestFitAllocator — per-core heterogeneous
// ============================================================

TEST(PartitionedEdfTest, BF_PerCore_Hetero_PacksTightest) {
    Engine engine;
    auto plat = HeterogeneousPerCorePlatform::create(engine);
    // big[0]: U=0.6 (remain=0.4), big[1]: U=0.2 (remain=0.8), little[0]: empty (remain=1.0)
    auto& tb0 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(6.0));
    auto& tb1 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    // Target U=0.1: scaled on big = 0.05, scaled on little = 0.2; all admissible
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    plat.schedulers[0]->add_server(tb0);  // big[0] remain=0.4
    plat.schedulers[1]->add_server(tb1);  // big[1] remain=0.8

    BestFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // BF picks tightest fit: big[0] remain=0.4, which is less than big[1]=0.8
    // and all empty cores=1.0
    EXPECT_NE(plat.schedulers[0]->find_server(target), nullptr);
}

TEST(PartitionedEdfTest, BF_PerCore_Hetero_SkipsScaledUtilViolation) {
    Engine engine;
    auto plat = HeterogeneousPerCorePlatform::create(engine);
    // Fill all big cores
    std::vector<Task*> fillers;
    for (int i = 0; i < 4; ++i) {
        fillers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    // Task with U=0.6: scaled_util on little = 0.6 * (2000/1000) / 1.0 = 1.2 > 1.0
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(6.0));
    engine.platform().finalize();

    for (int i = 0; i < 4; ++i) {
        plat.schedulers[i]->add_server(*fillers[i]);
    }

    BestFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(6.0));
    engine.run(time_from_seconds(0.5));

    // Big cores full, little cores reject due to scaled_util → task rejected
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(plat.schedulers[i]->find_server(target), nullptr);
    }
}

// ============================================================
// BestFitAllocator — per-cluster
// ============================================================

TEST(PartitionedEdfTest, BF_PerCluster_PacksIntoFullerCluster) {
    Engine engine;
    auto plat = BigLittlePerClusterPlatform::create(engine);
    // Big: U=3.8 (remain=0.2), Little: U=0.5 (remain=3.5)
    // Place 19 tasks U=0.2 on big
    std::vector<Task*> big_fillers;
    for (int i = 0; i < 19; ++i) {
        big_fillers.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0)));
    }
    // Place 1 task U=0.5 on little
    auto& little_filler = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));

    // Target U=0.1
    auto& target = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    for (auto* f : big_fillers) {
        plat.big_sched->add_server(*f);
    }
    plat.little_sched->add_server(little_filler);

    BestFitAllocator alloc(engine, plat.clusters_big_first());
    engine.schedule_job_arrival(target, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // Big has less remaining (0.2 < 3.5), tighter fit
    EXPECT_NE(plat.big_sched->find_server(target), nullptr);
    EXPECT_EQ(plat.little_sched->find_server(target), nullptr);
}

// ============================================================
// Admission edge cases
// ============================================================

TEST(PartitionedEdfTest, Admission_PerCore_ExactlyOne) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine, 1);
    // U = 1.0 exactly
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(10.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_NE(plat.schedulers[0]->find_server(task), nullptr);
}

TEST(PartitionedEdfTest, Admission_PerCore_OverOne) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine, 1);
    // U slightly over 1.0
    auto& task = engine.platform().add_task(
        duration_from_seconds(1000000.0), duration_from_seconds(1000000.0),
        duration_from_seconds(1000001.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1000001.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_EQ(plat.schedulers[0]->find_server(task), nullptr);
}

TEST(PartitionedEdfTest, Admission_PerCore_TwoTasksSumToOne) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine, 1);
    auto& t1 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(6.0));
    auto& t2 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(4.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(t1, time_from_seconds(0.0), duration_from_seconds(6.0));
    engine.schedule_job_arrival(t2, time_from_seconds(0.0), duration_from_seconds(4.0));
    engine.run(time_from_seconds(0.5));

    EXPECT_NE(plat.schedulers[0]->find_server(t1), nullptr);
    EXPECT_NE(plat.schedulers[0]->find_server(t2), nullptr);
}

TEST(PartitionedEdfTest, Admission_PerCore_TwoTasksExceedOne) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine, 1);
    auto& t1 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(6.0));
    auto& t2 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(t1, time_from_seconds(0.0), duration_from_seconds(6.0));
    engine.schedule_job_arrival(t2, time_from_seconds(0.0), duration_from_seconds(5.0));
    engine.run(time_from_seconds(0.5));

    // First admitted, second rejected
    EXPECT_NE(plat.schedulers[0]->find_server(t1), nullptr);
    EXPECT_EQ(plat.schedulers[0]->find_server(t2), nullptr);
}

// ============================================================
// Trace output
// ============================================================

TEST(PartitionedEdfTest, Trace_PerCore_TaskPlacedHasCpu) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    schedsim::io::MemoryTraceWriter trace_writer;
    engine.set_trace_writer(&trace_writer);

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    // Find the task_placed record
    bool found = false;
    for (const auto& rec : trace_writer.records()) {
        if (rec.type == "task_placed") {
            found = true;
            auto cpu_it = rec.fields.find("cpu");
            ASSERT_NE(cpu_it, rec.fields.end()) << "task_placed should have 'cpu' field";
            // Processor ID of first core
            auto cpu_val = std::get<uint64_t>(cpu_it->second);
            EXPECT_EQ(cpu_val, plat.procs[0]->id());
            break;
        }
    }
    EXPECT_TRUE(found) << "Should find a task_placed trace record";
}

TEST(PartitionedEdfTest, Trace_PerCluster_TaskPlacedNoCpu) {
    Engine engine;
    auto plat = BigLittlePerClusterPlatform::create(engine);
    auto& task = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    schedsim::io::MemoryTraceWriter trace_writer;
    engine.set_trace_writer(&trace_writer);

    FFBigFirstAllocator alloc(engine, plat.clusters_big_first());
    engine.schedule_job_arrival(task, time_from_seconds(0.0), duration_from_seconds(1.0));
    engine.run(time_from_seconds(0.5));

    bool found = false;
    for (const auto& rec : trace_writer.records()) {
        if (rec.type == "task_placed") {
            found = true;
            EXPECT_NE(rec.fields.find("cluster_id"), rec.fields.end());
            EXPECT_EQ(rec.fields.find("cpu"), rec.fields.end())
                << "Per-cluster task_placed should not have 'cpu' field";
            break;
        }
    }
    EXPECT_TRUE(found) << "Should find a task_placed trace record";
}

// ============================================================
// Per-core cluster vs ClockDomain (regression guard)
// ============================================================

TEST(PartitionedEdfTest, PerCoreCluster_ProcessorCountIsOne) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    // Add 4 processors to same clock domain
    for (int i = 0; i < 4; ++i) {
        engine.platform().add_processor(pt, cd, pd);
    }
    engine.platform().finalize();

    // Create a single-proc scheduler with only the first processor
    auto& proc = engine.platform().processor(0);
    EdfScheduler sched(engine, {&proc});
    Cluster cluster(cd, sched, 1.0, 2000.0);

    // Should return 1 (from scheduler), not 4 (from clock domain)
    EXPECT_EQ(cluster.processor_count(), 1u);
}

TEST(PartitionedEdfTest, PerCoreCluster_RemainingCapacityIsOne) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    for (int i = 0; i < 4; ++i) {
        engine.platform().add_processor(pt, cd, pd);
    }
    engine.platform().finalize();

    auto& proc = engine.platform().processor(0);
    EdfScheduler sched(engine, {&proc});
    Cluster cluster(cd, sched, 1.0, 2000.0);

    EXPECT_DOUBLE_EQ(cluster.remaining_capacity(), 1.0);
}

// ============================================================
// Integration (end-to-end)
// ============================================================

TEST(PartitionedEdfTest, Integration_PerCore_TasksComplete) {
    Engine engine;
    auto plat = UniformPerCorePlatform::create(engine);
    // 4 tasks, U=0.5, period=10, wcet=5
    std::vector<Task*> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0)));
    }
    engine.platform().finalize();

    schedsim::io::MemoryTraceWriter trace_writer;
    engine.set_trace_writer(&trace_writer);

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);

    // Schedule 2 arrivals per task: t=0 and t=10
    for (auto* t : tasks) {
        engine.schedule_job_arrival(*t, time_from_seconds(0.0), t->wcet());
        engine.schedule_job_arrival(*t, time_from_seconds(10.0), t->wcet());
    }

    engine.run(time_from_seconds(25.0));

    // Count job completions
    std::size_t completions = 0;
    for (const auto& rec : trace_writer.records()) {
        if (rec.type == "job_finished") {
            ++completions;
        }
    }
    EXPECT_EQ(completions, 8u);  // 4 tasks × 2 jobs

    // All processors should be idle at end
    for (auto* proc : plat.procs) {
        EXPECT_EQ(proc->state(), ProcessorState::Idle);
    }
}

TEST(PartitionedEdfTest, Integration_PerCore_Hetero_MixedPlacement) {
    Engine engine;
    auto plat = HeterogeneousPerCorePlatform::create(engine);
    // 4 "big" tasks U=1.0 — completely fill big cores, too heavy for little cores
    // (scaled_util on little = 1.0 * (2000/1000) / 1.0 = 2.0 > 1.0)
    std::vector<Task*> big_tasks;
    for (int i = 0; i < 4; ++i) {
        big_tasks.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0)));
    }
    // 4 "little" tasks U=0.1 each — small enough to work on little cores
    // (scaled_util on little = 0.1 * 2.0 / 1.0 = 0.2; wall-clock U = 0.2 per task)
    // Big cores are full (U=1.0), so FF routes little tasks to little cores
    std::vector<Task*> little_tasks;
    for (int i = 0; i < 4; ++i) {
        little_tasks.push_back(&engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0)));
    }
    engine.platform().finalize();

    schedsim::io::MemoryTraceWriter trace_writer;
    engine.set_trace_writer(&trace_writer);

    FirstFitAllocator alloc(engine, plat.cluster_ptrs);

    // Schedule 2 arrivals per task: t=0 and t=10
    for (auto* t : big_tasks) {
        engine.schedule_job_arrival(*t, time_from_seconds(0.0), t->wcet());
        engine.schedule_job_arrival(*t, time_from_seconds(10.0), t->wcet());
    }
    for (auto* t : little_tasks) {
        engine.schedule_job_arrival(*t, time_from_seconds(0.0), t->wcet());
        engine.schedule_job_arrival(*t, time_from_seconds(10.0), t->wcet());
    }

    engine.run(time_from_seconds(25.0));

    // Big tasks should be on big cores (schedulers 0-3), one per core
    for (int i = 0; i < 4; ++i) {
        EXPECT_NE(plat.schedulers[i]->find_server(*big_tasks[i]), nullptr)
            << "Big task " << i << " should be on big core " << i;
    }

    // Little tasks should be on little cores (schedulers 4-7), not on big cores
    for (int i = 0; i < 4; ++i) {
        bool on_little = false;
        for (int j = 4; j < 8; ++j) {
            if (plat.schedulers[j]->find_server(*little_tasks[i]) != nullptr) {
                on_little = true;
                break;
            }
        }
        EXPECT_TRUE(on_little)
            << "Little task " << i << " should be on a little core";
        for (int j = 0; j < 4; ++j) {
            EXPECT_EQ(plat.schedulers[j]->find_server(*little_tasks[i]), nullptr)
                << "Little task " << i << " should not be on big core " << j;
        }
    }

    // Count completions: 8 tasks × 2 jobs = 16
    std::size_t completions = 0;
    for (const auto& rec : trace_writer.records()) {
        if (rec.type == "job_finished") {
            ++completions;
        }
    }
    EXPECT_EQ(completions, 16u);
}
