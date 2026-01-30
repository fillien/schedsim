#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

class ContextSwitchTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create processor type with 0.1s context switch delay
        auto& pt = engine_.platform().add_processor_type("big", 1.0, Duration{0.1});
        auto& cd = engine_.platform().add_clock_domain(Frequency{1000.0}, Frequency{2000.0});
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
        });
        proc_ = &engine_.platform().add_processor(pt, cd, pd);
        engine_.platform().finalize();

        task_ = std::make_unique<Task>(0, Duration{10.0}, Duration{10.0}, Duration{2.0});
    }

    Engine engine_;
    Processor* proc_{nullptr};
    std::unique_ptr<Task> task_;
};

TEST_F(ContextSwitchTest, CSDisabledByDefault) {
    // Context switch is disabled by default
    EXPECT_FALSE(engine_.context_switch_enabled());
}

TEST_F(ContextSwitchTest, CSDisabledSkipsDelay) {
    // With CS disabled, assign goes directly to Running
    TimePoint deadline{Duration{10.0}};
    Job job(*task_, Duration{2.0}, deadline);

    proc_->assign(job);

    EXPECT_EQ(proc_->state(), ProcessorState::Running);
    EXPECT_EQ(proc_->current_job(), &job);
}

TEST_F(ContextSwitchTest, CSEnabledTransitionsToContextSwitching) {
    engine_.enable_context_switch(true);

    TimePoint deadline{Duration{10.0}};
    Job job(*task_, Duration{2.0}, deadline);

    proc_->assign(job);

    EXPECT_EQ(proc_->state(), ProcessorState::ContextSwitching);
    EXPECT_EQ(proc_->current_job(), nullptr);  // Job not assigned yet
}

TEST_F(ContextSwitchTest, CSCompletesAfterDelay) {
    engine_.enable_context_switch(true);

    TimePoint completion_time;
    proc_->set_processor_available_handler([&](Processor&) {
        completion_time = engine_.time();
    });

    TimePoint deadline{Duration{10.0}};
    Job job(*task_, Duration{2.0}, deadline);

    proc_->assign(job);
    EXPECT_EQ(proc_->state(), ProcessorState::ContextSwitching);

    // Run past the context switch delay (0.1s)
    engine_.run(TimePoint{Duration{0.15}});

    EXPECT_EQ(proc_->state(), ProcessorState::Running);
    EXPECT_EQ(proc_->current_job(), &job);
    // Context switch completes at exactly 0.1s
    EXPECT_DOUBLE_EQ(completion_time.time_since_epoch().count(), 0.1);
}

TEST_F(ContextSwitchTest, ProcessorAvailableISRFiredAfterCS) {
    engine_.enable_context_switch(true);

    bool isr_fired = false;
    proc_->set_processor_available_handler([&](Processor& p) {
        isr_fired = true;
        EXPECT_EQ(&p, proc_);
        EXPECT_EQ(p.state(), ProcessorState::Running);
    });

    TimePoint deadline{Duration{10.0}};
    Job job(*task_, Duration{2.0}, deadline);

    proc_->assign(job);
    engine_.run(TimePoint{Duration{0.15}});

    EXPECT_TRUE(isr_fired);
}

TEST_F(ContextSwitchTest, ClearDuringCS) {
    engine_.enable_context_switch(true);

    TimePoint deadline{Duration{10.0}};
    Job job(*task_, Duration{2.0}, deadline);

    proc_->assign(job);
    EXPECT_EQ(proc_->state(), ProcessorState::ContextSwitching);

    // Clear during context switching
    proc_->clear();

    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
    EXPECT_EQ(proc_->current_job(), nullptr);

    // Run simulation - should have no effect (timer was cancelled)
    engine_.run(TimePoint{Duration{0.2}});

    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
}

TEST_F(ContextSwitchTest, JobTimingWithCS) {
    engine_.enable_context_switch(true);

    bool completion_called = false;
    TimePoint completion_time;

    proc_->set_job_completion_handler([&](Processor&, Job&) {
        completion_called = true;
        completion_time = engine_.time();
    });

    TimePoint deadline{Duration{10.0}};
    Job job(*task_, Duration{2.0}, deadline);  // 2.0 reference units

    proc_->assign(job);

    // Run until completion
    // CS delay: 0.1s, Job: 2.0s at speed 1.0 = 2.0s wall time
    // Total: 2.1s
    engine_.run(TimePoint{Duration{3.0}});

    EXPECT_TRUE(completion_called);
    EXPECT_DOUBLE_EQ(completion_time.time_since_epoch().count(), 2.1);
}

TEST_F(ContextSwitchTest, AssignDuringCSThrows) {
    engine_.enable_context_switch(true);

    TimePoint deadline{Duration{10.0}};
    Job job1(*task_, Duration{2.0}, deadline);
    Job job2(*task_, Duration{2.0}, deadline);

    proc_->assign(job1);
    EXPECT_EQ(proc_->state(), ProcessorState::ContextSwitching);

    // Trying to assign another job during CS should throw
    EXPECT_THROW(proc_->assign(job2), InvalidStateError);
}

TEST_F(ContextSwitchTest, ZeroDelaySkipsCS) {
    // Create a new processor type with zero CS delay
    Engine engine2;
    auto& pt = engine2.platform().add_processor_type("small", 0.5, Duration{0.0});
    auto& cd = engine2.platform().add_clock_domain(Frequency{1000.0}, Frequency{2000.0});
    auto& pd = engine2.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{50.0}}
    });
    auto& proc = engine2.platform().add_processor(pt, cd, pd);
    engine2.platform().finalize();

    engine2.enable_context_switch(true);

    Task task(0, Duration{10.0}, Duration{10.0}, Duration{2.0});
    TimePoint deadline{Duration{10.0}};
    Job job(task, Duration{2.0}, deadline);

    proc.assign(job);

    // Even with CS enabled, zero delay goes directly to Running
    EXPECT_EQ(proc.state(), ProcessorState::Running);
}

TEST_F(ContextSwitchTest, DeadlineMissDuringCS) {
    engine_.enable_context_switch(true);
    // CS delay is 0.1s (from SetUp)

    bool deadline_miss_called = false;
    TimePoint miss_time;
    proc_->set_deadline_miss_handler([&](Processor& p, Job& j) {
        deadline_miss_called = true;
        miss_time = engine_.time();
        EXPECT_EQ(&p, proc_);
        (void)j;
    });

    // Job with deadline at 0.05s - BEFORE context switch completes (0.1s)
    TimePoint deadline{Duration{0.05}};
    Job job(*task_, Duration{2.0}, deadline);

    proc_->assign(job);  // Starts context switch
    EXPECT_EQ(proc_->state(), ProcessorState::ContextSwitching);

    // Run past the deadline (but before CS completes)
    engine_.run(TimePoint{Duration{0.2}});

    // Deadline miss MUST be detected
    EXPECT_TRUE(deadline_miss_called);
    // Miss detected after CS completes (0.1s), not at actual deadline (0.05s)
    // This is acceptable - we detect it once job reaches Running state
    EXPECT_GE(miss_time.time_since_epoch().count(), 0.1);
}
