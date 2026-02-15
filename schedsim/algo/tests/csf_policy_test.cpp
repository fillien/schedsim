#include <schedsim/algo/csf_policy.hpp>

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
class CsfPolicyTest : public ::testing::Test {
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

TEST_F(CsfPolicyTest, ZeroUtilization_MinActiveProcs) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    CsfPolicy policy(engine_);

    policy.on_utilization_changed(sched, *clock_domain_);

    // With zero util, max_util=0: m_min = ceil((0-0)/(1-0)) = 0 → clamped to 1
    // freq_min = 2000 * (0 + 0*0)/1 = 0 → < freq_eff
    // active = ceil(1 * 0 / 1000) = 0 → clamped to 1
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 1000.0);
}

TEST_F(CsfPolicyTest, HighUtilization_AllCoresMaxFreq) {
    auto& task1 = engine_.platform().add_task(Duration{1.0}, Duration{1.0}, Duration{1.0});
    auto& task2 = engine_.platform().add_task(Duration{1.0}, Duration{1.0}, Duration{1.0});
    auto& task3 = engine_.platform().add_task(Duration{1.0}, Duration{1.0}, Duration{1.0});
    auto& task4 = engine_.platform().add_task(Duration{1.0}, Duration{1.0}, Duration{1.0});
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    sched.add_server(task1, Duration{1.0}, Duration{1.0});
    sched.add_server(task2, Duration{1.0}, Duration{1.0});
    sched.add_server(task3, Duration{1.0}, Duration{1.0});
    sched.add_server(task4, Duration{1.0}, Duration{1.0});

    CsfPolicy policy(engine_);

    policy.on_utilization_changed(sched, *clock_domain_);

    // max_util = 1.0 → m_min = total_procs = 4
    // freq_min = 2000 * (4 + 3*1)/4 = 3500 → clamped to 2000
    // 2000 >= freq_eff → all cores, ceil_to_mode(2000) = 2000
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 2000.0);
}

TEST_F(CsfPolicyTest, MaxUtilOne_NoZeroDivision) {
    // Test the max_util >= 1.0 guard
    auto& task = engine_.platform().add_task(Duration{1.0}, Duration{1.0}, Duration{1.0});
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    sched.add_server(task, Duration{1.0}, Duration{1.0});

    CsfPolicy policy(engine_);

    // Should not crash (max_util=1.0 → division-by-zero guard)
    policy.on_utilization_changed(sched, *clock_domain_);
    SUCCEED();
}

TEST_F(CsfPolicyTest, MediumUtilization_ReducesCores) {
    // 2 tasks, each util=0.3, total=0.6, max=0.3
    auto& task1 = engine_.platform().add_task(Duration{10.0}, Duration{3.0}, Duration{3.0});
    auto& task2 = engine_.platform().add_task(Duration{10.0}, Duration{3.0}, Duration{3.0});
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    sched.add_server(task1, Duration{3.0}, Duration{10.0});
    sched.add_server(task2, Duration{3.0}, Duration{10.0});

    CsfPolicy policy(engine_);

    policy.on_utilization_changed(sched, *clock_domain_);

    // m_min = ceil((0.6 - 0.3) / (1 - 0.3)) = ceil(0.4286) = 1
    // freq_min = 2000 * (0.6 + 0*0.3)/1 = 1200
    // 1200 >= freq_eff(1000) → all cores at ceil_to_mode(1200) = 1500
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 1500.0);
}

TEST_F(CsfPolicyTest, LowUtilization_ReducesCoresAndFrequency) {
    // 1 task, util=0.1
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{1.0}, Duration{1.0});
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    sched.add_server(task, Duration{1.0}, Duration{10.0});

    CsfPolicy policy(engine_);

    policy.on_utilization_changed(sched, *clock_domain_);

    // m_min = ceil((0.1 - 0.1) / (1 - 0.1)) = ceil(0) = 0 → clamped to 1
    // freq_min = 2000 * (0.1 + 0*0.1)/1 = 200
    // 200 < freq_eff(1000) → freq_eff, active = ceil(1 * 200/1000) = ceil(0.2) = 1
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 1000.0);

    int sleeping = 0;
    for (int i = 0; i < 4; ++i) {
        if (procs_[i]->state() == ProcessorState::Sleep) {
            ++sleeping;
        }
    }
    EXPECT_EQ(sleeping, 3);
}

TEST_F(CsfPolicyTest, CooldownPreventsFrequencyThrashing) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    CsfPolicy policy(engine_, Duration{1.0});

    policy.on_utilization_changed(sched, *clock_domain_);
    Frequency first_freq = clock_domain_->frequency();

    clock_domain_->set_frequency(Frequency{2000.0});

    policy.on_utilization_changed(sched, *clock_domain_);
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, 2000.0);

    engine_.run(TimePoint{Duration{1.5}});

    policy.on_utilization_changed(sched, *clock_domain_);
    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, first_freq.mhz);
}

TEST_F(CsfPolicyTest, LockedDomainSkipped) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    CsfPolicy policy(engine_);

    clock_domain_->lock_frequency();
    Frequency locked_freq = clock_domain_->frequency();

    policy.on_utilization_changed(sched, *clock_domain_);

    EXPECT_DOUBLE_EQ(clock_domain_->frequency().mhz, locked_freq.mhz);
}

TEST_F(CsfPolicyTest, EnableCsfConvenience) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    sched.enable_csf(Duration{0.5}, 1);

    SUCCEED();
}

TEST_F(CsfPolicyTest, OnProcessorIdleIsNoOp) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    CsfPolicy policy(engine_);

    policy.on_processor_idle(sched, *procs_[0]);
    EXPECT_EQ(procs_[0]->state(), ProcessorState::Idle);
}
