#include <schedsim/algo/dpm_policy.hpp>

#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class DpmPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("cpu", 1.0);
        auto& cd = engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
        // Add multiple C-states for DPM testing
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}},  // C0 (active)
            {1, CStateScope::PerProcessor, Duration{0.001}, Power{50.0}}, // C1
            {2, CStateScope::PerProcessor, Duration{0.01}, Power{10.0}}   // C2
        });
        proc_ = &engine_.platform().add_processor(pt, cd, pd);
        // Don't finalize here - tests will finalize after adding tasks if needed
    }

    Engine engine_;
    Processor* proc_{nullptr};
};

TEST_F(DpmPolicyTest, DefaultParameters) {
    BasicDpmPolicy policy;

    EXPECT_EQ(policy.target_cstate(), 1);
    EXPECT_DOUBLE_EQ(policy.idle_threshold().count(), 0.0);
    EXPECT_EQ(policy.sleeping_processor_count(), 0U);
}

TEST_F(DpmPolicyTest, CustomParameters) {
    BasicDpmPolicy policy(2, Duration{0.5});

    EXPECT_EQ(policy.target_cstate(), 2);
    EXPECT_DOUBLE_EQ(policy.idle_threshold().count(), 0.5);
}

TEST_F(DpmPolicyTest, OnProcessorIdle_PutsToSleep) {
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    BasicDpmPolicy policy(1);

    // Processor should be idle initially
    EXPECT_EQ(proc_->state(), ProcessorState::Idle);

    policy.on_processor_idle(sched, *proc_);

    // Processor should now be in sleep state
    EXPECT_EQ(proc_->state(), ProcessorState::Sleep);
    EXPECT_EQ(policy.sleeping_processor_count(), 1U);
}

TEST_F(DpmPolicyTest, OnProcessorIdle_AlreadySleeping) {
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    BasicDpmPolicy policy(1);

    // Put to sleep first
    policy.on_processor_idle(sched, *proc_);
    EXPECT_EQ(policy.sleeping_processor_count(), 1U);

    // Calling again should not increase count
    policy.on_processor_idle(sched, *proc_);
    EXPECT_EQ(policy.sleeping_processor_count(), 1U);
}

TEST_F(DpmPolicyTest, OnProcessorNeeded_CleansUpWokenProcessors) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    BasicDpmPolicy policy(1);

    // Put to sleep
    policy.on_processor_idle(sched, *proc_);
    EXPECT_EQ(policy.sleeping_processor_count(), 1U);

    // Manually wake the processor (simulating job assignment)
    Job job(task, Duration{2.0}, TimePoint{Duration{10.0}});
    proc_->assign(job);
    engine_.run(TimePoint{Duration{0.001}});  // Let wake-up complete

    // Now notify policy - should clean up
    policy.on_processor_needed(sched);

    // Sleeping count should be 0 now
    EXPECT_EQ(policy.sleeping_processor_count(), 0U);
}

TEST_F(DpmPolicyTest, TargetCState) {
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    BasicDpmPolicy policy(2);  // Target C2

    policy.on_processor_idle(sched, *proc_);

    EXPECT_EQ(proc_->state(), ProcessorState::Sleep);
    EXPECT_EQ(proc_->current_cstate_level(), 2);
}
