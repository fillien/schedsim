#include <schedsim/core/platform.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

class PlatformTest : public ::testing::Test {
protected:
    Engine engine_;
};

TEST_F(PlatformTest, AddProcessorType) {
    auto& pt = engine_.platform().add_processor_type("big", 1.5);

    EXPECT_EQ(pt.id(), 0U);
    EXPECT_EQ(pt.name(), "big");
    EXPECT_DOUBLE_EQ(pt.performance(), 1.5);
}

TEST_F(PlatformTest, AddMultipleProcessorTypes) {
    auto& pt1 = engine_.platform().add_processor_type("big", 1.5);
    auto& pt2 = engine_.platform().add_processor_type("LITTLE", 0.5);

    EXPECT_EQ(pt1.id(), 0U);
    EXPECT_EQ(pt2.id(), 1U);
    EXPECT_EQ(engine_.platform().processor_type_count(), 2U);
}

TEST_F(PlatformTest, AddClockDomain) {
    auto& cd = engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});

    EXPECT_EQ(cd.id(), 0U);
    EXPECT_EQ(cd.freq_min().mhz, 500.0);
    EXPECT_EQ(cd.freq_max().mhz, 2000.0);
}

TEST_F(PlatformTest, AddClockDomainWithDelay) {
    auto& cd = engine_.platform().add_clock_domain(
        Frequency{500.0}, Frequency{2000.0}, duration_from_seconds(0.001));

    EXPECT_EQ(duration_to_seconds(cd.transition_delay()), 0.001);
}

TEST_F(PlatformTest, AddPowerDomain) {
    std::vector<CStateLevel> c_states = {
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
        {1, CStateScope::PerProcessor, duration_from_seconds(0.001), Power{50.0}},
    };

    auto& pd = engine_.platform().add_power_domain(c_states);

    EXPECT_EQ(pd.id(), 0U);
    EXPECT_EQ(pd.c_states().size(), 2U);
}

TEST_F(PlatformTest, AddProcessor) {
    auto& pt = engine_.platform().add_processor_type("big", 1.0);
    auto& cd = engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine_.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    auto& proc = engine_.platform().add_processor(pt, cd, pd);

    EXPECT_EQ(proc.id(), 0U);
    EXPECT_EQ(&proc.type(), &pt);
    EXPECT_EQ(&proc.clock_domain(), &cd);
    EXPECT_EQ(&proc.power_domain(), &pd);
}

TEST_F(PlatformTest, ProcessorWiredToClockDomain) {
    auto& pt = engine_.platform().add_processor_type("big", 1.0);
    auto& cd = engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine_.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    auto& proc = engine_.platform().add_processor(pt, cd, pd);

    EXPECT_EQ(cd.processors().size(), 1U);
    EXPECT_EQ(cd.processors()[0], &proc);
}

TEST_F(PlatformTest, ProcessorWiredToPowerDomain) {
    auto& pt = engine_.platform().add_processor_type("big", 1.0);
    auto& cd = engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine_.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    auto& proc = engine_.platform().add_processor(pt, cd, pd);

    EXPECT_EQ(pd.processors().size(), 1U);
    EXPECT_EQ(pd.processors()[0], &proc);
}

TEST_F(PlatformTest, AddTask) {
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));

    EXPECT_EQ(task.id(), 0U);
    EXPECT_EQ(duration_to_seconds(task.period()), 10.0);
    EXPECT_EQ(duration_to_seconds(task.relative_deadline()), 10.0);
    EXPECT_EQ(duration_to_seconds(task.wcet()), 2.0);
}

TEST_F(PlatformTest, Finalize) {
    engine_.platform().add_processor_type("big", 1.0);

    EXPECT_FALSE(engine_.platform().is_finalized());
    engine_.platform().finalize();
    EXPECT_TRUE(engine_.platform().is_finalized());
}

TEST_F(PlatformTest, FinalizeIsIdempotent) {
    engine_.platform().add_processor_type("big", 1.0);

    engine_.platform().finalize();
    engine_.platform().finalize();  // Should not throw

    EXPECT_TRUE(engine_.platform().is_finalized());
}

TEST_F(PlatformTest, AddAfterFinalizeThrows) {
    engine_.platform().finalize();

    EXPECT_THROW(
        engine_.platform().add_processor_type("big", 1.0),
        AlreadyFinalizedError);

    EXPECT_THROW(
        engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0}),
        AlreadyFinalizedError);

    EXPECT_THROW(
        engine_.platform().add_power_domain({}),
        AlreadyFinalizedError);

    EXPECT_THROW(
        engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0)),
        AlreadyFinalizedError);
}

TEST_F(PlatformTest, ReferencePerformanceSingleType) {
    engine_.platform().add_processor_type("big", 1.5);
    engine_.platform().finalize();

    EXPECT_DOUBLE_EQ(engine_.platform().reference_performance(), 1.5);
}

TEST_F(PlatformTest, ReferencePerformanceMultipleTypes) {
    engine_.platform().add_processor_type("LITTLE", 0.5);
    engine_.platform().add_processor_type("big", 1.5);
    engine_.platform().add_processor_type("medium", 1.0);
    engine_.platform().finalize();

    EXPECT_DOUBLE_EQ(engine_.platform().reference_performance(), 1.5);
}

TEST_F(PlatformTest, ReferencePerformanceNoTypes) {
    engine_.platform().finalize();

    // Default is 1.0 when no processor types defined
    EXPECT_DOUBLE_EQ(engine_.platform().reference_performance(), 1.0);
}

TEST_F(PlatformTest, ProcessorSpeedUsesReferencePerformance) {
    auto& pt = engine_.platform().add_processor_type("big", 2.0);
    auto& cd = engine_.platform().add_clock_domain(Frequency{1000.0}, Frequency{2000.0});
    auto& pd = engine_.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    auto& proc = engine_.platform().add_processor(pt, cd, pd);
    engine_.platform().finalize();

    // Reference = 2.0, type = 2.0, freq = 2000/2000
    // Speed = (2000/2000) * (2.0/2.0) = 1.0
    EXPECT_DOUBLE_EQ(proc.speed(engine_.platform().reference_performance()), 1.0);
}

TEST_F(PlatformTest, MultipleProcessorsSameClockDomain) {
    auto& pt = engine_.platform().add_processor_type("big", 1.0);
    auto& cd = engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine_.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    auto& proc1 = engine_.platform().add_processor(pt, cd, pd);
    auto& proc2 = engine_.platform().add_processor(pt, cd, pd);

    EXPECT_EQ(cd.processors().size(), 2U);
    EXPECT_EQ(cd.processors()[0], &proc1);
    EXPECT_EQ(cd.processors()[1], &proc2);
}

TEST_F(PlatformTest, SpanAccessors) {
    engine_.platform().add_processor_type("big", 1.0);
    engine_.platform().add_processor_type("LITTLE", 0.5);
    engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    engine_.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));

    EXPECT_EQ(engine_.platform().processor_type_count(), 2U);
    EXPECT_EQ(engine_.platform().clock_domain_count(), 1U);
    EXPECT_EQ(engine_.platform().power_domain_count(), 1U);
    EXPECT_EQ(engine_.platform().task_count(), 1U);
}

// Job arrival integration test
TEST_F(PlatformTest, JobArrivalScheduling) {
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine_.platform().finalize();

    bool handler_called = false;
    Task* arrived_task = nullptr;

    engine_.set_job_arrival_handler([&](Task& t, Job job) {
        handler_called = true;
        arrived_task = &t;
        EXPECT_DOUBLE_EQ(duration_to_seconds(job.total_work()), 2.0);
        EXPECT_DOUBLE_EQ(time_to_seconds(job.absolute_deadline()), 15.0);  // 5 + 10
    });

    engine_.schedule_job_arrival(task, time_from_seconds(5.0), duration_from_seconds(2.0));
    engine_.run();

    EXPECT_TRUE(handler_called);
    EXPECT_EQ(arrived_task, &task);
}

TEST_F(PlatformTest, HandlerAlreadySetThrows) {
    engine_.set_job_arrival_handler([](Task&, Job) {});

    EXPECT_THROW(
        engine_.set_job_arrival_handler([](Task&, Job) {}),
        HandlerAlreadySetError);
}

// =============================================================================
// add_task with Explicit ID Tests
// =============================================================================

TEST_F(PlatformTest, AddTaskWithExplicitId) {
    auto& task = engine_.platform().add_task(42, duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    EXPECT_EQ(task.id(), 42U);
    EXPECT_EQ(duration_to_seconds(task.period()), 10.0);
    EXPECT_EQ(duration_to_seconds(task.wcet()), 2.0);
}

TEST_F(PlatformTest, AddTaskWithExplicitId_NonSequential) {
    auto& t1 = engine_.platform().add_task(5, duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    auto& t2 = engine_.platform().add_task(10, duration_from_seconds(20.0), duration_from_seconds(20.0), duration_from_seconds(3.0));
    auto& t3 = engine_.platform().add_task(1, duration_from_seconds(5.0), duration_from_seconds(5.0), duration_from_seconds(1.0));

    EXPECT_EQ(t1.id(), 5U);
    EXPECT_EQ(t2.id(), 10U);
    EXPECT_EQ(t3.id(), 1U);
    EXPECT_EQ(engine_.platform().task_count(), 3U);
}
