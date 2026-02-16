#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

class CStateWakeupTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("big", 1.0);
        auto& cd = engine_.platform().add_clock_domain(Frequency{1000.0}, Frequency{2000.0});
        pd_ = &engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},    // C0 - active
            {1, CStateScope::PerProcessor, duration_from_seconds(0.01), Power{50.0}},   // C1 - light sleep
            {2, CStateScope::PerProcessor, duration_from_seconds(0.05), Power{10.0}},   // C2 - deep sleep
            {3, CStateScope::DomainWide, duration_from_seconds(0.1), Power{1.0}}        // C3 - package sleep
        });
        proc_ = &engine_.platform().add_processor(pt, cd, *pd_);
        engine_.platform().finalize();

        task_ = std::make_unique<Task>(0, duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    }

    Engine engine_;
    PowerDomain* pd_{nullptr};
    Processor* proc_{nullptr};
    std::unique_ptr<Task> task_;
};

TEST_F(CStateWakeupTest, WakeLatencyC0) {
    // C0 (active) has no wake latency
    EXPECT_DOUBLE_EQ(duration_to_seconds(pd_->wake_latency(0)), 0.0);
}

TEST_F(CStateWakeupTest, WakeLatencyC1) {
    EXPECT_DOUBLE_EQ(duration_to_seconds(pd_->wake_latency(1)), 0.01);
}

TEST_F(CStateWakeupTest, WakeLatencyC2) {
    EXPECT_DOUBLE_EQ(duration_to_seconds(pd_->wake_latency(2)), 0.05);
}

TEST_F(CStateWakeupTest, WakeLatencyC3) {
    EXPECT_DOUBLE_EQ(duration_to_seconds(pd_->wake_latency(3)), 0.1);
}

TEST_F(CStateWakeupTest, WakeLatencyUnknown) {
    // Unknown level returns 0
    EXPECT_DOUBLE_EQ(duration_to_seconds(pd_->wake_latency(99)), 0.0);
}

TEST_F(CStateWakeupTest, CStatePower) {
    EXPECT_DOUBLE_EQ(pd_->cstate_power(0).mw, 100.0);
    EXPECT_DOUBLE_EQ(pd_->cstate_power(1).mw, 50.0);
    EXPECT_DOUBLE_EQ(pd_->cstate_power(2).mw, 10.0);
    EXPECT_DOUBLE_EQ(pd_->cstate_power(3).mw, 1.0);
    EXPECT_DOUBLE_EQ(pd_->cstate_power(99).mw, 0.0);  // Unknown
}

TEST_F(CStateWakeupTest, RequestCStateStoresLevel) {
    EXPECT_EQ(proc_->current_cstate_level(), 0);

    proc_->request_cstate(2);

    EXPECT_EQ(proc_->state(), ProcessorState::Sleep);
    EXPECT_EQ(proc_->current_cstate_level(), 2);
}

TEST_F(CStateWakeupTest, AssignOnSleepingTriggersWakeup) {
    proc_->request_cstate(1);
    EXPECT_EQ(proc_->state(), ProcessorState::Sleep);

    TimePoint deadline = time_from_seconds(10.0);
    Job job(*task_, duration_from_seconds(2.0), deadline);

    proc_->assign(job);

    // Processor is still in Sleep state until wake-up completes
    EXPECT_EQ(proc_->state(), ProcessorState::Sleep);
}

TEST_F(CStateWakeupTest, WakeupCompletesAfterLatency) {
    proc_->request_cstate(1);  // C1 has 0.01s wake latency

    TimePoint deadline = time_from_seconds(10.0);
    Job job(*task_, duration_from_seconds(2.0), deadline);

    proc_->assign(job);

    // Run past wake-up latency
    engine_.run(time_from_seconds(0.02));

    // Should be Running (went through Idle and possibly CS)
    EXPECT_EQ(proc_->state(), ProcessorState::Running);
    EXPECT_EQ(proc_->current_cstate_level(), 0);
}

TEST_F(CStateWakeupTest, ProcessorAvailableISRFiredAfterWakeup) {
    proc_->request_cstate(1);

    bool isr_fired = false;
    proc_->set_processor_available_handler([&](Processor& p) {
        isr_fired = true;
        EXPECT_EQ(&p, proc_);
    });

    TimePoint deadline = time_from_seconds(10.0);
    Job job(*task_, duration_from_seconds(2.0), deadline);

    proc_->assign(job);
    engine_.run(time_from_seconds(0.02));

    EXPECT_TRUE(isr_fired);
}

TEST_F(CStateWakeupTest, WakeupThenCSEnabled) {
    // Enable context switch (processor type has 0 delay, so need new setup)
    Engine engine2;
    auto& pt = engine2.platform().add_processor_type("big", 1.0, duration_from_seconds(0.02));
    auto& cd = engine2.platform().add_clock_domain(Frequency{1000.0}, Frequency{2000.0});
    auto& pd = engine2.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
        {1, CStateScope::PerProcessor, duration_from_seconds(0.01), Power{50.0}}
    });
    auto& proc = engine2.platform().add_processor(pt, cd, pd);
    engine2.platform().finalize();
    engine2.enable_context_switch(true);

    proc.request_cstate(1);  // C1: 0.01s wake latency

    Task task(0, duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    TimePoint deadline = time_from_seconds(10.0);
    Job job(task, duration_from_seconds(2.0), deadline);

    proc.assign(job);

    // After wake-up (0.01s), should be in ContextSwitching
    engine2.run(time_from_seconds(0.015));
    EXPECT_EQ(proc.state(), ProcessorState::ContextSwitching);

    // After CS (0.01 + 0.02 = 0.03s), should be Running
    engine2.run(time_from_seconds(0.04));
    EXPECT_EQ(proc.state(), ProcessorState::Running);
}

TEST_F(CStateWakeupTest, DeepSleepTakesLonger) {
    proc_->request_cstate(2);  // C2 has 0.05s wake latency

    bool completion_called = false;
    TimePoint completion_time;

    proc_->set_job_completion_handler([&](Processor&, Job&) {
        completion_called = true;
        completion_time = engine_.time();
    });

    TimePoint deadline = time_from_seconds(10.0);
    Job job(*task_, duration_from_seconds(2.0), deadline);

    proc_->assign(job);

    // Run until completion
    // Wake-up: 0.05s, Job: 2.0s at speed 1.0 = 2.0s
    // Total: 2.05s
    engine_.run(time_from_seconds(3.0));

    EXPECT_TRUE(completion_called);
    EXPECT_NEAR(time_to_seconds(completion_time), 2.05, 0.001);
}

TEST_F(CStateWakeupTest, AchievedCStatePerProcessor) {
    // Per-processor C-state (C1, C2) can be achieved individually
    proc_->request_cstate(2);

    int achieved = pd_->achieved_cstate_for_processor(*proc_);
    EXPECT_EQ(achieved, 2);
}

TEST_F(CStateWakeupTest, ZeroLatencyWakeUp) {
    // Zero-latency C-state wake-up: assign on sleeping proc → immediate Running
    Engine engine2;
    auto& pt = engine2.platform().add_processor_type("big", 1.0);
    auto& cd = engine2.platform().add_clock_domain(Frequency{1000.0}, Frequency{2000.0});
    auto& pd = engine2.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
        {1, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{50.0}}  // C1 with zero wake latency
    });
    auto& proc = engine2.platform().add_processor(pt, cd, pd);
    engine2.platform().finalize();

    proc.request_cstate(1);
    EXPECT_EQ(proc.state(), ProcessorState::Sleep);

    Task task(0, duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    TimePoint deadline = time_from_seconds(10.0);
    Job job(task, duration_from_seconds(2.0), deadline);

    proc.assign(job);

    // Zero latency → immediate transition to Running (no WakeUp state)
    EXPECT_EQ(proc.state(), ProcessorState::Running);
    EXPECT_EQ(proc.current_job(), &job);
    EXPECT_EQ(proc.current_cstate_level(), 0);
}

TEST_F(CStateWakeupTest, AchievedCStateDomainWide) {
    // Create two processors in the same power domain
    Engine engine2;
    auto& pt = engine2.platform().add_processor_type("big", 1.0);
    auto& cd = engine2.platform().add_clock_domain(Frequency{1000.0}, Frequency{2000.0});
    auto& pd = engine2.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
        {1, CStateScope::PerProcessor, duration_from_seconds(0.01), Power{50.0}},
        {3, CStateScope::DomainWide, duration_from_seconds(0.1), Power{1.0}}
    });
    auto& proc1 = engine2.platform().add_processor(pt, cd, pd);
    auto& proc2 = engine2.platform().add_processor(pt, cd, pd);
    engine2.platform().finalize();

    // Only proc1 requests C3 - domain-wide C-state won't be achieved
    proc1.request_cstate(3);

    // proc2 is still active (C0), so domain-wide C3 is not achievable
    int achieved = pd.achieved_cstate_for_processor(proc1);
    EXPECT_EQ(achieved, 0);  // Falls back to C0

    // Now proc2 also enters sleep at C3
    proc2.request_cstate(3);

    achieved = pd.achieved_cstate_for_processor(proc1);
    EXPECT_EQ(achieved, 3);  // Both requested C3, so domain-wide C3 is achieved

    achieved = pd.achieved_cstate_for_processor(proc2);
    EXPECT_EQ(achieved, 3);
}
