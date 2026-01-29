#include <schedsim/algo/dvfs_policy.hpp>

#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class DvfsPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("cpu", 1.0);
        clock_domain_ = &engine_.platform().add_clock_domain(
            Frequency{500.0}, Frequency{2000.0});
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
        });
        proc_ = &engine_.platform().add_processor(pt, *clock_domain_, pd);
        engine_.platform().finalize();
    }

    Engine engine_;
    Processor* proc_{nullptr};
    ClockDomain* clock_domain_{nullptr};
};

TEST_F(DvfsPolicyTest, CooldownTimer_CanChange_Initially) {
    CooldownTimer timer(engine_, Duration{1.0});
    EXPECT_TRUE(timer.can_change());
    EXPECT_FALSE(timer.in_cooldown());
}

TEST_F(DvfsPolicyTest, CooldownTimer_StartCooldown) {
    CooldownTimer timer(engine_, Duration{1.0});

    timer.start_cooldown();
    EXPECT_FALSE(timer.can_change());
    EXPECT_TRUE(timer.in_cooldown());

    // Advance time past cooldown
    engine_.run(TimePoint{Duration{1.5}});
    EXPECT_TRUE(timer.can_change());
    EXPECT_FALSE(timer.in_cooldown());
}

TEST_F(DvfsPolicyTest, PowerAware_InitialFrequency) {
    // Verify initial frequency is at max
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 2000.0);
}

TEST_F(DvfsPolicyTest, PowerAware_FrequencyScaling) {
    EdfScheduler sched(engine_, {proc_});
    sched.enable_grub();  // Enable GRUB for active utilization tracking

    PowerAwareDvfsPolicy policy(engine_);

    // With no servers, active util = 0, frequency should go to f_min
    policy.on_utilization_changed(sched, *clock_domain_);
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 500.0);
}

TEST_F(DvfsPolicyTest, PowerAware_FrequencyCallbackInvoked) {
    EdfScheduler sched(engine_, {proc_});

    PowerAwareDvfsPolicy policy(engine_);

    bool callback_invoked = false;
    ClockDomain* callback_domain = nullptr;
    policy.set_frequency_changed_callback([&](ClockDomain& domain) {
        callback_invoked = true;
        callback_domain = &domain;
    });

    // This should change frequency and invoke callback
    policy.on_utilization_changed(sched, *clock_domain_);

    EXPECT_TRUE(callback_invoked);
    EXPECT_EQ(callback_domain, clock_domain_);
}

TEST_F(DvfsPolicyTest, PowerAware_CooldownPreventsChange) {
    EdfScheduler sched(engine_, {proc_});

    // 1 second cooldown
    PowerAwareDvfsPolicy policy(engine_, Duration{1.0});

    // First change should work
    policy.on_utilization_changed(sched, *clock_domain_);
    Frequency first_freq = clock_domain_->frequency();

    // Manually set frequency back to max (simulating external change)
    clock_domain_->set_frequency(Frequency{2000.0});

    // Immediate second change should be blocked by cooldown
    policy.on_utilization_changed(sched, *clock_domain_);
    // Frequency should remain at 2000 because cooldown is active
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 2000.0);

    // Advance time past cooldown
    engine_.run(TimePoint{Duration{1.5}});

    // Now change should work
    policy.on_utilization_changed(sched, *clock_domain_);
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, first_freq.mhz);
}

TEST_F(DvfsPolicyTest, PowerAware_CooldownPeriod) {
    PowerAwareDvfsPolicy policy(engine_, Duration{2.5});
    EXPECT_DOUBLE_EQ(policy.cooldown_period().count(), 2.5);
}

TEST_F(DvfsPolicyTest, PowerAware_LockedDomainSkipped) {
    EdfScheduler sched(engine_, {proc_});
    PowerAwareDvfsPolicy policy(engine_);

    Frequency original_freq = clock_domain_->frequency();

    // Lock the domain
    clock_domain_->lock_frequency();

    // Try to change - should be skipped
    policy.on_utilization_changed(sched, *clock_domain_);

    // Frequency should not change
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, original_freq.mhz);
}
