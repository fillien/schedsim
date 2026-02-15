#include <schedsim/algo/ffa_policy.hpp>

#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/grub_policy.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

// Multi-processor fixture with discrete frequency modes and freq_eff
class FfaPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("cpu", 1.0);
        clock_domain_ = &engine_.platform().add_clock_domain(
            Frequency{200.0}, Frequency{2000.0});
        clock_domain_->set_frequency_modes({
            Frequency{200.0}, Frequency{500.0}, Frequency{800.0},
            Frequency{1000.0}, Frequency{1500.0}, Frequency{2000.0}
        });
        clock_domain_->set_freq_eff(Frequency{1000.0});

        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}},
            {1, CStateScope::PerProcessor, Duration{0.001}, Power{10.0}}
        });

        for (int i = 0; i < 4; ++i) {
            procs_[i] = &engine_.platform().add_processor(pt, *clock_domain_, pd);
        }
    }

    Engine engine_;
    Processor* procs_[4]{};
    ClockDomain* clock_domain_{nullptr};
};

TEST_F(FfaPolicyTest, ZeroUtilization_MinFrequency) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    FfaPolicy policy(engine_);

    policy.on_utilization_changed(sched, *clock_domain_);

    // With zero utilization, freq_min formula gives 0 which is < freq_eff
    // So we use freq_eff and reduce cores. 0 utilization -> 0 cores -> clamped to 1.
    // Frequency should be ceil_to_mode(freq_eff) = 1000
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 1000.0);
}

TEST_F(FfaPolicyTest, HighUtilization_MaxFrequency) {
    // Add tasks consuming full utilization on 4 procs
    auto& task1 = engine_.platform().add_task(Duration{1.0}, Duration{1.0}, Duration{1.0});
    auto& task2 = engine_.platform().add_task(Duration{1.0}, Duration{1.0}, Duration{1.0});
    auto& task3 = engine_.platform().add_task(Duration{1.0}, Duration{1.0}, Duration{1.0});
    auto& task4 = engine_.platform().add_task(Duration{1.0}, Duration{1.0}, Duration{1.0});
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    // No GRUB: active_utilization() returns total_utilization (sum of server U_i)

    // Add servers with utilization = 1.0 each, total = 4.0
    sched.add_server(task1, Duration{1.0}, Duration{1.0});
    sched.add_server(task2, Duration{1.0}, Duration{1.0});
    sched.add_server(task3, Duration{1.0}, Duration{1.0});
    sched.add_server(task4, Duration{1.0}, Duration{1.0});

    FfaPolicy policy(engine_);

    policy.on_utilization_changed(sched, *clock_domain_);

    // active_util = 4.0, max_util = 1.0, m = 4
    // freq_min = 2000 * (4.0 + 3*1.0) / 4 = 3500 → clamped to 2000
    // 2000 >= freq_eff(1000) → all cores at ceil_to_mode(2000) = 2000
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 2000.0);
}

TEST_F(FfaPolicyTest, MediumUtilization_ReducedFrequency) {
    // 4 tasks, each util=0.2, total=0.8, max=0.2
    auto& task1 = engine_.platform().add_task(Duration{10.0}, Duration{2.0}, Duration{2.0});
    auto& task2 = engine_.platform().add_task(Duration{10.0}, Duration{2.0}, Duration{2.0});
    auto& task3 = engine_.platform().add_task(Duration{10.0}, Duration{2.0}, Duration{2.0});
    auto& task4 = engine_.platform().add_task(Duration{10.0}, Duration{2.0}, Duration{2.0});
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    sched.add_server(task1, Duration{2.0}, Duration{10.0});
    sched.add_server(task2, Duration{2.0}, Duration{10.0});
    sched.add_server(task3, Duration{2.0}, Duration{10.0});
    sched.add_server(task4, Duration{2.0}, Duration{10.0});

    FfaPolicy policy(engine_);

    policy.on_utilization_changed(sched, *clock_domain_);

    // active_util = 0.8, max_util = 0.2, m = 4
    // freq_min = 2000 * (0.8 + 3*0.2)/4 = 2000 * 1.4/4 = 700
    // 700 < freq_eff(1000) → use freq_eff, reduce cores
    // active = ceil(4 * 700 / 1000) = ceil(2.8) = 3
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 1000.0);
}

TEST_F(FfaPolicyTest, LowUtilization_ReducedCores) {
    // 1 task, util=0.1, total=0.1, max=0.1
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{1.0}, Duration{1.0});
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    sched.add_server(task, Duration{1.0}, Duration{10.0});

    FfaPolicy policy(engine_);

    policy.on_utilization_changed(sched, *clock_domain_);

    // Total util = 0.1, max_util = 0.1, m = 4
    // freq_min = 2000 * (0.1 + 3*0.1)/4 = 2000 * 0.4/4 = 200
    // 200 < freq_eff(1000) → use freq_eff, reduce cores
    // active = ceil(4 * 200 / 1000) = ceil(0.8) = 1
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 1000.0);

    // At least 1 processor should remain active/idle, others asleep
    int sleeping = 0;
    for (int i = 0; i < 4; ++i) {
        if (procs_[i]->state() == ProcessorState::Sleep) {
            ++sleeping;
        }
    }
    EXPECT_EQ(sleeping, 3);
}

TEST_F(FfaPolicyTest, CooldownPreventsFrequencyThrashing) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    FfaPolicy policy(engine_, Duration{1.0});

    // First call should change frequency
    policy.on_utilization_changed(sched, *clock_domain_);
    Frequency first_freq = clock_domain_->frequency();

    // Reset frequency manually
    clock_domain_->set_frequency(Frequency{2000.0});

    // Second call should be blocked by cooldown
    policy.on_utilization_changed(sched, *clock_domain_);
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 2000.0);

    // Advance past cooldown
    engine_.run(TimePoint{Duration{1.5}});

    // Now it should work
    policy.on_utilization_changed(sched, *clock_domain_);
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, first_freq.mhz);
}

TEST_F(FfaPolicyTest, LockedDomainSkipped) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    FfaPolicy policy(engine_);

    clock_domain_->lock_frequency();
    Frequency locked_freq = clock_domain_->frequency();

    policy.on_utilization_changed(sched, *clock_domain_);

    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, locked_freq.mhz);
}

TEST_F(FfaPolicyTest, OnProcessorIdleIsNoOp) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    FfaPolicy policy(engine_);

    // Should not throw or change anything
    policy.on_processor_idle(sched, *procs_[0]);
    EXPECT_EQ(procs_[0]->state(), ProcessorState::Idle);
}

TEST_F(FfaPolicyTest, OnProcessorActiveIsNoOp) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    FfaPolicy policy(engine_);

    // Should not throw
    policy.on_processor_active(sched, *procs_[0]);
}

TEST_F(FfaPolicyTest, EnableFfaConvenience) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    sched.enable_ffa(Duration{0.5}, 1);

    // Verify it's operational
    SUCCEED();
}

TEST_F(FfaPolicyTest, NoFreqEff_UsesAllCores) {
    // Remove freq_eff (set to 0)
    clock_domain_->set_freq_eff(Frequency{0.0});

    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{1.0}, Duration{1.0});
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    sched.add_server(task, Duration{1.0}, Duration{10.0});

    FfaPolicy policy(engine_);

    policy.on_utilization_changed(sched, *clock_domain_);

    // With no freq_eff, the "freq_min < freq_eff" branch is never taken (freq_eff=0)
    // So all 4 cores stay active, frequency = ceil_to_mode(200) = 200
    int sleeping = 0;
    for (int i = 0; i < 4; ++i) {
        if (procs_[i]->state() == ProcessorState::Sleep) {
            ++sleeping;
        }
    }
    EXPECT_EQ(sleeping, 0);
}

TEST_F(FfaPolicyTest, FrequencyCallbackInvoked) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    FfaPolicy policy(engine_);

    bool callback_invoked = false;
    policy.set_frequency_changed_callback([&](ClockDomain&) {
        callback_invoked = true;
    });

    policy.on_utilization_changed(sched, *clock_domain_);

    EXPECT_TRUE(callback_invoked);
}
