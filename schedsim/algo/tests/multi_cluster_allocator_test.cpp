#include <schedsim/algo/cluster.hpp>
#include <schedsim/algo/multi_cluster_allocator.hpp>

#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

// Concrete test allocator: always picks the first cluster that can admit
class BigFirstAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

protected:
    Cluster* select_cluster(const Task& /*task*/) override {
        for (auto* cluster : clusters()) {
            return cluster;  // always pick first (big)
        }
        return nullptr;
    }
};

// Allocator that always rejects
class RejectAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

protected:
    Cluster* select_cluster(const Task& /*task*/) override {
        return nullptr;
    }
};

class MultiClusterAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("big", 1.0);
        cd_ = &engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });
        for (int i = 0; i < 2; ++i) {
            procs_[i] = &engine_.platform().add_processor(pt, *cd_, pd);
        }
    }

    TimePoint time(double seconds) {
        return time_from_seconds(seconds);
    }

    Engine engine_;
    ClockDomain* cd_{nullptr};
    Processor* procs_[2]{};
};

TEST_F(MultiClusterAllocatorTest, RoutesToSelectedCluster) {
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 2);
    EdfScheduler sched(engine_, proc_vec);
    Cluster cluster(*cd_, sched, 1.0, 2000.0);

    BigFirstAllocator alloc(engine_, {&cluster});

    engine_.schedule_job_arrival(task, time(0.0), duration_from_seconds(2.0));
    engine_.run(time(0.5));

    // Job should be running on one of the cluster's processors
    bool running = false;
    for (int i = 0; i < 2; ++i) {
        if (procs_[i]->state() == ProcessorState::Running) {
            running = true;
        }
    }
    EXPECT_TRUE(running);
}

TEST_F(MultiClusterAllocatorTest, RejectsWhenNoCluster) {
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 2);
    EdfScheduler sched(engine_, proc_vec);
    Cluster cluster(*cd_, sched, 1.0, 2000.0);

    RejectAllocator alloc(engine_, {&cluster});

    engine_.schedule_job_arrival(task, time(0.0), duration_from_seconds(2.0));
    engine_.run(time(0.5));

    // No job should be running (rejected)
    for (int i = 0; i < 2; ++i) {
        EXPECT_NE(procs_[i]->state(), ProcessorState::Running);
    }
}

TEST_F(MultiClusterAllocatorTest, SubsequentJobsSameCluster) {
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 2);
    EdfScheduler sched(engine_, proc_vec);
    Cluster cluster(*cd_, sched, 1.0, 2000.0);

    BigFirstAllocator alloc(engine_, {&cluster});

    // Schedule two job arrivals for the same task
    engine_.schedule_job_arrival(task, time(0.0), duration_from_seconds(1.0));
    engine_.schedule_job_arrival(task, time(5.0), duration_from_seconds(1.0));

    engine_.run(time(10.0));

    // Both should complete, processor should be idle
    bool all_idle = true;
    for (int i = 0; i < 2; ++i) {
        if (procs_[i]->state() == ProcessorState::Running) {
            all_idle = false;
        }
    }
    EXPECT_TRUE(all_idle);
}

TEST_F(MultiClusterAllocatorTest, CreatesServerOnFirstArrival) {
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 2);
    EdfScheduler sched(engine_, proc_vec);
    Cluster cluster(*cd_, sched, 1.0, 2000.0);

    BigFirstAllocator alloc(engine_, {&cluster});

    // No server yet
    EXPECT_EQ(sched.server_count(), 0u);

    engine_.schedule_job_arrival(task, time(0.0), duration_from_seconds(2.0));
    engine_.run(time(0.5));

    // EdfScheduler auto-creates a CBS server on first job arrival
    EXPECT_EQ(sched.server_count(), 1u);
}

TEST_F(MultiClusterAllocatorTest, HandlerAlreadySetError) {
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 2);
    EdfScheduler sched(engine_, proc_vec);
    Cluster cluster(*cd_, sched, 1.0, 2000.0);

    BigFirstAllocator alloc1(engine_, {&cluster});

    // Second allocator on same engine should fail
    EXPECT_THROW(
        BigFirstAllocator(engine_, {&cluster}),
        HandlerAlreadySetError
    );
}
