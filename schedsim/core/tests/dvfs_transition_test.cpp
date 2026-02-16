#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

class DVFSTransitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clock domain with 0.05s transition delay
        auto& pt = engine_.platform().add_processor_type("big", 1.0);
        cd_ = &engine_.platform().add_clock_domain(
            Frequency{500.0}, Frequency{2000.0}, Duration{0.05});
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
        });
        proc_ = &engine_.platform().add_processor(pt, *cd_, pd);
        engine_.platform().finalize();

        task_ = std::make_unique<Task>(0, Duration{10.0}, Duration{10.0}, Duration{2.0});
    }

    Engine engine_;
    ClockDomain* cd_{nullptr};
    Processor* proc_{nullptr};
    std::unique_ptr<Task> task_;
};

TEST_F(DVFSTransitionTest, InstantChangeNoDelay) {
    // Create a clock domain without delay
    Engine engine2;
    auto& pt = engine2.platform().add_processor_type("big", 1.0);
    auto& cd = engine2.platform().add_clock_domain(
        Frequency{500.0}, Frequency{2000.0}, Duration{0.0});
    auto& pd = engine2.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto& proc = engine2.platform().add_processor(pt, cd, pd);
    engine2.platform().finalize();

    EXPECT_DOUBLE_EQ(cd.frequency().mhz, 2000.0);
    EXPECT_FALSE(cd.is_transitioning());

    cd.set_frequency(Frequency{1000.0});

    EXPECT_DOUBLE_EQ(cd.frequency().mhz, 1000.0);
    EXPECT_FALSE(cd.is_transitioning());
    EXPECT_EQ(proc.state(), ProcessorState::Idle);
}

TEST_F(DVFSTransitionTest, InstantChangeNoDelay_WhileRunning) {
    // Zero-delay DVFS on a Running processor exercises notify_immediate_freq_change()
    Engine engine2;
    auto& pt2 = engine2.platform().add_processor_type("big", 1.0);
    auto& cd2 = engine2.platform().add_clock_domain(
        Frequency{500.0}, Frequency{2000.0}, Duration{0.0});
    auto& pd2 = engine2.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto& proc2 = engine2.platform().add_processor(pt2, cd2, pd2);
    engine2.platform().finalize();

    bool completion_called = false;
    TimePoint completion_time;
    proc2.set_job_completion_handler([&](Processor&, Job&) {
        completion_called = true;
        completion_time = engine2.time();
    });

    Task task2(0, Duration{10.0}, Duration{10.0}, Duration{2.0});
    TimePoint deadline{Duration{10.0}};
    Job job(task2, Duration{2.0}, deadline);

    proc2.assign(job);
    EXPECT_EQ(proc2.state(), ProcessorState::Running);

    // At t=1.0, change frequency to half (instant, no Changing state)
    engine2.add_timer(TimePoint{Duration{1.0}}, [&]() {
        cd2.set_frequency(Frequency{1000.0});
    });

    engine2.run(TimePoint{Duration{10.0}});

    EXPECT_TRUE(completion_called);
    EXPECT_TRUE(job.is_complete());
    EXPECT_EQ(proc2.state(), ProcessorState::Idle);
    // Zero-delay path: frequency changes before update_consumed_work runs,
    // so elapsed 1.0s is accounted at new speed 0.5 → 0.5 work done.
    // Remaining 1.5 at speed 0.5 → 3.0s more. Completion at t=4.0.
    EXPECT_NEAR(completion_time.time_since_epoch().count(), 4.0, 0.001);
}

TEST_F(DVFSTransitionTest, DelayedChangeStartsTransition) {
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, 2000.0);
    EXPECT_FALSE(cd_->is_transitioning());

    cd_->set_frequency(Frequency{1000.0});

    // Transition should be in progress
    EXPECT_TRUE(cd_->is_transitioning());
    // Frequency hasn't changed yet
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, 2000.0);
    // Processor should be in Changing state
    EXPECT_EQ(proc_->state(), ProcessorState::Changing);
}

TEST_F(DVFSTransitionTest, TransitionCompletesAfterDelay) {
    cd_->set_frequency(Frequency{1000.0});
    EXPECT_TRUE(cd_->is_transitioning());

    // Run past the transition delay (0.05s)
    engine_.run(TimePoint{Duration{0.1}});

    EXPECT_FALSE(cd_->is_transitioning());
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, 1000.0);
    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
}

TEST_F(DVFSTransitionTest, ProcessorAvailableISRFiredAfterDVFS) {
    bool isr_fired = false;
    proc_->set_processor_available_handler([&](Processor& p) {
        isr_fired = true;
        EXPECT_EQ(&p, proc_);
    });

    cd_->set_frequency(Frequency{1000.0});
    engine_.run(TimePoint{Duration{0.1}});

    EXPECT_TRUE(isr_fired);
}

TEST_F(DVFSTransitionTest, DVFSOnRunningProcessor) {
    bool completion_called = false;
    TimePoint completion_time;

    proc_->set_job_completion_handler([&](Processor&, Job&) {
        completion_called = true;
        completion_time = engine_.time();
    });

    TimePoint deadline{Duration{10.0}};
    Job job(*task_, Duration{2.0}, deadline);

    proc_->assign(job);
    EXPECT_EQ(proc_->state(), ProcessorState::Running);

    // Run for 0.5s, then trigger DVFS
    engine_.run(TimePoint{Duration{0.5}});

    // Change to half frequency - this triggers update_consumed_work()
    cd_->set_frequency(Frequency{1000.0});  // Half speed

    // At speed 1.0, 0.5s elapsed = 0.5 work done, 1.5 remaining
    EXPECT_NEAR(job.remaining_work().count(), 1.5, 0.001);
    EXPECT_EQ(proc_->state(), ProcessorState::Changing);

    // Run until transition completes
    engine_.run(TimePoint{Duration{0.6}});

    EXPECT_EQ(proc_->state(), ProcessorState::Running);
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, 1000.0);

    // Remaining work is 1.5, at speed 0.5, wall time = 3.0s
    // From t=0.55 (transition complete), completion at t=3.55
    engine_.run(TimePoint{Duration{4.0}});

    EXPECT_TRUE(completion_called);
    // Work: 0.5s at speed 1.0 = 0.5, DVFS delay 0.05s, remaining 1.5 at speed 0.5 = 3.0s
    // Total: 0.5 + 0.05 + 3.0 = 3.55s
    EXPECT_NEAR(completion_time.time_since_epoch().count(), 3.55, 0.001);
}

TEST_F(DVFSTransitionTest, ClearDuringChanging) {
    TimePoint deadline{Duration{10.0}};
    Job job(*task_, Duration{2.0}, deadline);

    proc_->assign(job);
    EXPECT_EQ(proc_->state(), ProcessorState::Running);

    // Start DVFS
    cd_->set_frequency(Frequency{1000.0});
    EXPECT_EQ(proc_->state(), ProcessorState::Changing);

    // Clear during Changing
    proc_->clear();

    // State is still Changing until transition completes
    EXPECT_EQ(proc_->state(), ProcessorState::Changing);

    // Run until transition completes
    engine_.run(TimePoint{Duration{0.1}});

    // After DVFS completes, processor should be Idle (not Running)
    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
    EXPECT_EQ(proc_->current_job(), nullptr);
}

TEST_F(DVFSTransitionTest, DVFSWhileTransitioningThrows) {
    cd_->set_frequency(Frequency{1000.0});
    EXPECT_TRUE(cd_->is_transitioning());

    // Trying to change frequency during transition should throw
    EXPECT_THROW(cd_->set_frequency(Frequency{1500.0}), InvalidStateError);
}

TEST_F(DVFSTransitionTest, MultipleProcessorsDVFS) {
    // Create a second processor in the same clock domain
    auto& pt = engine_.platform().processor_type(0);
    auto& pd = engine_.platform().power_domain(0);

    // Need to recreate since finalize was called
    Engine engine2;
    auto& pt2 = engine2.platform().add_processor_type("big", 1.0);
    auto& cd2 = engine2.platform().add_clock_domain(
        Frequency{500.0}, Frequency{2000.0}, Duration{0.05});
    auto& pd2 = engine2.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto& proc1 = engine2.platform().add_processor(pt2, cd2, pd2);
    auto& proc2 = engine2.platform().add_processor(pt2, cd2, pd2);
    engine2.platform().finalize();

    EXPECT_EQ(proc1.state(), ProcessorState::Idle);
    EXPECT_EQ(proc2.state(), ProcessorState::Idle);

    cd2.set_frequency(Frequency{1000.0});

    // Both processors should be in Changing state
    EXPECT_EQ(proc1.state(), ProcessorState::Changing);
    EXPECT_EQ(proc2.state(), ProcessorState::Changing);

    engine2.run(TimePoint{Duration{0.1}});

    // Both should be back to Idle
    EXPECT_EQ(proc1.state(), ProcessorState::Idle);
    EXPECT_EQ(proc2.state(), ProcessorState::Idle);
}

TEST_F(DVFSTransitionTest, PowerCoefficients) {
    cd_->set_power_coefficients({10.0, 50.0, 100.0, 0.0});

    // P(f) = 10 + 50*f + 100*f^2, f in GHz
    // At 2.0 GHz: P = 10 + 50*2 + 100*4 = 10 + 100 + 400 = 510 mW
    Power p = cd_->power_at_frequency(Frequency{2000.0});
    EXPECT_NEAR(p.mw, 510.0, 0.001);

    // At 1.0 GHz: P = 10 + 50*1 + 100*1 = 160 mW
    p = cd_->power_at_frequency(Frequency{1000.0});
    EXPECT_NEAR(p.mw, 160.0, 0.001);
}
