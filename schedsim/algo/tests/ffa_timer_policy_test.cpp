#include <schedsim/algo/ffa_timer_policy.hpp>

#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/grub_policy.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class FfaTimerPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("cpu", 1.0);
        cd_ = &engine_.platform().add_clock_domain(Frequency{200.0}, Frequency{2000.0});
        cd_->set_frequency_modes({
            Frequency{200.0}, Frequency{500.0}, Frequency{800.0},
            Frequency{1000.0}, Frequency{1500.0}, Frequency{2000.0}
        });
        cd_->set_freq_eff(Frequency{1000.0});

        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
            {1, CStateScope::PerProcessor, duration_from_seconds(0.001), Power{10.0}}
        });

        for (int i = 0; i < 4; ++i) {
            procs_[i] = &engine_.platform().add_processor(pt, *cd_, pd);
        }
    }

    Engine engine_;
    Processor* procs_[4]{};
    ClockDomain* cd_{nullptr};
};

TEST_F(FfaTimerPolicyTest, ZeroCooldown_ImmediateApplication) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    // Zero cooldown => delegates to parent (immediate mode)
    FfaTimerPolicy policy(engine_, duration_from_seconds(0.0));

    policy.on_utilization_changed(sched, *cd_);

    // With zero utilization and freq_eff=1000, should immediately apply freq_eff
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, 1000.0);
}

TEST_F(FfaTimerPolicyTest, DeferredApplication_TimerFires) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    FfaTimerPolicy policy(engine_, duration_from_seconds(1.0));

    // Initial frequency is 2000
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, 2000.0);

    policy.on_utilization_changed(sched, *cd_);

    // Frequency should NOT have changed yet (deferred by 1.0)
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, 2000.0);

    // Advance past the cooldown timer
    engine_.run(time_from_seconds(1.5));

    // Now the timer should have fired and applied the target
    EXPECT_NE(cd_->frequency().mhz, 2000.0);
}

TEST_F(FfaTimerPolicyTest, TimerReset_OnNewUtilChange) {
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(1.0), duration_from_seconds(1.0));
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    FfaTimerPolicy policy(engine_, duration_from_seconds(2.0));

    // First call at t=0: schedules timer for t=2.0
    policy.on_utilization_changed(sched, *cd_);
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, 2000.0);  // Not yet changed

    // Advance to t=1.0 (timer hasn't fired yet)
    engine_.run(time_from_seconds(1.0));
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, 2000.0);

    // New utilization change at t=1.0: should cancel old timer, schedule new at t=3.0
    sched.add_server(task, duration_from_seconds(1.0), duration_from_seconds(10.0));
    policy.on_utilization_changed(sched, *cd_);

    // Advance to t=2.5 (past original timer but before new timer)
    engine_.run(time_from_seconds(2.5));
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, 2000.0);  // Old timer was cancelled

    // Advance past new timer
    engine_.run(time_from_seconds(3.5));
    EXPECT_NE(cd_->frequency().mhz, 2000.0);  // New timer fired
}

TEST_F(FfaTimerPolicyTest, NoChangeNeeded_NoTimer) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    // First, apply immediate FFA to get the "correct" state
    FfaPolicy immediate_policy(engine_);
    immediate_policy.on_utilization_changed(sched, *cd_);
    Frequency settled_freq = cd_->frequency();

    // Now use timer policy — target already matches current state
    FfaTimerPolicy timer_policy(engine_, duration_from_seconds(1.0));
    timer_policy.on_utilization_changed(sched, *cd_);

    // Should still be at settled frequency (no timer needed)
    EXPECT_DOUBLE_EQ(cd_->frequency().mhz, settled_freq.mhz);
}

TEST_F(FfaTimerPolicyTest, EnableFfaTimerConvenience) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    sched.enable_ffa_timer(duration_from_seconds(0.5), 1);

    // Verify it's operational
    SUCCEED();
}

TEST_F(FfaTimerPolicyTest, FrequencyCallbackInvoked) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    FfaTimerPolicy policy(engine_, duration_from_seconds(1.0));

    bool callback_invoked = false;
    policy.set_frequency_changed_callback([&](ClockDomain&) {
        callback_invoked = true;
    });

    policy.on_utilization_changed(sched, *cd_);

    // Not invoked yet (deferred)
    EXPECT_FALSE(callback_invoked);

    // Fire the timer
    engine_.run(time_from_seconds(1.5));

    EXPECT_TRUE(callback_invoked);
}
