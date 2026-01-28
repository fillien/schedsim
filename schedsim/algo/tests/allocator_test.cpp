#include <schedsim/algo/single_scheduler_allocator.hpp>
#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/platform.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class AllocatorTestBase : public ::testing::Test {
protected:
    TimePoint time(double seconds) {
        return TimePoint{Duration{seconds}};
    }
};

TEST_F(AllocatorTestBase, SingleSchedulerAllocator_Construction) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    SingleSchedulerAllocator alloc(engine, sched);

    // Just verify construction doesn't throw
    SUCCEED();
}

TEST_F(AllocatorTestBase, SingleSchedulerAllocator_RoutesToScheduler) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    sched.add_server(task);
    SingleSchedulerAllocator alloc(engine, sched);

    // Schedule a job arrival
    engine.schedule_job_arrival(task, time(0.0), Duration{2.0});

    // Before running, no job is active
    EXPECT_EQ(proc->current_job(), nullptr);

    // Run to process the arrival
    engine.run(time(0.5));

    // Job should be running on the processor
    EXPECT_EQ(proc->state(), ProcessorState::Running);
    EXPECT_NE(proc->current_job(), nullptr);
}

TEST_F(AllocatorTestBase, HandlerAlreadySetError) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    EdfScheduler sched1(engine, {proc});
    SingleSchedulerAllocator alloc1(engine, sched1);

    // Try to create another allocator - should fail
    EdfScheduler sched2(engine, {});  // Empty processor list for second scheduler
    EXPECT_THROW(
        SingleSchedulerAllocator(engine, sched2),
        HandlerAlreadySetError
    );
}

TEST_F(AllocatorTestBase, MultipleJobArrivals_AllRouted) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{1.0});
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    sched.add_server(task);
    SingleSchedulerAllocator alloc(engine, sched);

    // Schedule multiple job arrivals
    engine.schedule_job_arrival(task, time(0.0), Duration{1.0});
    engine.schedule_job_arrival(task, time(5.0), Duration{1.0});
    engine.schedule_job_arrival(task, time(10.0), Duration{1.0});

    engine.run(time(15.0));

    // After all jobs complete, processor should be idle
    EXPECT_EQ(proc->state(), ProcessorState::Idle);
}

TEST_F(AllocatorTestBase, DifferentTasks_BothRouted) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task1 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{1.0});
    auto& task2 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{1.0});
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    sched.add_server(task1);
    sched.add_server(task2);
    SingleSchedulerAllocator alloc(engine, sched);

    engine.schedule_job_arrival(task1, time(0.0), Duration{1.0});
    engine.schedule_job_arrival(task2, time(0.0), Duration{1.0});

    engine.run(time(10.0));

    // Both jobs should complete (one after the other on single processor)
    EXPECT_EQ(proc->state(), ProcessorState::Idle);
}
