#include <schedsim/algo/csf_policy.hpp>
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

// Test-only subclasses to expose protected compute_target()
class TestableCsfPolicy : public CsfPolicy {
public:
    using CsfPolicy::CsfPolicy;
    using CsfPolicy::compute_target;
};

class TestableFfaPolicyForComparison : public FfaPolicy {
public:
    using FfaPolicy::FfaPolicy;
    using FfaPolicy::compute_target;
};

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

TEST_F(CsfPolicyTest, OnProcessorActiveIsNoOp) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);

    CsfPolicy policy(engine_);

    // Should not throw
    policy.on_processor_active(sched, *procs_[0]);
}

TEST_F(CsfPolicyTest, FrequencyCallbackInvoked) {
    engine_.platform().finalize();
    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    CsfPolicy policy(engine_);

    bool callback_invoked = false;
    policy.set_frequency_changed_callback([&](ClockDomain&) {
        callback_invoked = true;
    });

    policy.on_utilization_changed(sched, *clock_domain_);

    EXPECT_TRUE(callback_invoked);
}

TEST_F(CsfPolicyTest, NoFreqEff_UsesAllCores) {
    clock_domain_->set_freq_eff(Frequency{0.0});

    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{1.0}, Duration{1.0});
    engine_.platform().finalize();

    std::vector<Processor*> proc_vec(procs_, procs_ + 4);
    EdfScheduler sched(engine_, proc_vec);
    sched.enable_grub();

    sched.add_server(task, Duration{1.0}, Duration{10.0});

    CsfPolicy policy(engine_);

    policy.on_utilization_changed(sched, *clock_domain_);

    // With no freq_eff, the "freq_min < freq_eff" branch is never taken (freq_eff=0)
    // So all 4 cores stay active
    int sleeping = 0;
    for (int i = 0; i < 4; ++i) {
        if (procs_[i]->state() == ProcessorState::Sleep) {
            ++sleeping;
        }
    }
    EXPECT_EQ(sleeping, 0);
}

// ---------------------------------------------------------------------------
// Parametric compute_target() tests
// ---------------------------------------------------------------------------

TEST_F(CsfPolicyTest, ComputeTarget_MminClampedToOne) {
    engine_.platform().finalize();
    TestableCsfPolicy policy(engine_);

    // U_active=0.2, U_max=0.2, m=4
    // m_min = ceil((0.2-0.2)/(1-0.2)) = ceil(0) = 0 → clamp to 1
    // freq_min = 2000 * (0.2 + 0*0.2)/1 = 400
    // 400 < freq_eff(1000) → freq_eff, ceil(1*400/1000)=ceil(0.4)=1
    auto target = policy.compute_target(0.2, 0.2, 4, *clock_domain_);
    EXPECT_DOUBLE_EQ(target.frequency.mhz, 1000.0);
    EXPECT_EQ(target.active_processors, 1u);
}

TEST_F(CsfPolicyTest, ComputeTarget_MminTwo) {
    engine_.platform().finalize();
    TestableCsfPolicy policy(engine_);

    // U_active=1.5, U_max=0.4, m=4
    // m_min = ceil((1.5-0.4)/(1-0.4)) = ceil(1.833) = 2
    // freq_min = 2000 * (1.5 + 1*0.4)/2 = 2000 * 1.9/2 = 1900
    // 1900 >= freq_eff(1000) → else branch, active=total_procs=4
    // ceil_to_mode(1900) = 2000
    auto target = policy.compute_target(1.5, 0.4, 4, *clock_domain_);
    EXPECT_DOUBLE_EQ(target.frequency.mhz, 2000.0);
    EXPECT_EQ(target.active_processors, 4u);
}

TEST_F(CsfPolicyTest, ComputeTarget_MaxUtilGuard) {
    engine_.platform().finalize();
    TestableCsfPolicy policy(engine_);

    // U_active=2.0, U_max=1.0, m=4
    // max_util >= 1.0 → m_min = total_procs = 4
    // freq_min = 2000 * (2.0 + 3*1.0)/4 = 2500 → min(2500,2000)=2000
    // 2000 >= freq_eff(1000) → ceil_to_mode(2000)=2000, all 4 cores
    auto target = policy.compute_target(2.0, 1.0, 4, *clock_domain_);
    EXPECT_DOUBLE_EQ(target.frequency.mhz, 2000.0);
    EXPECT_EQ(target.active_processors, 4u);
}

TEST_F(CsfPolicyTest, ComputeTarget_MaxUtilNearOne) {
    engine_.platform().finalize();
    TestableCsfPolicy policy(engine_);

    // U_active=2.0, U_max=0.999, m=4
    // max_util < 1.0 → m_min = ceil((2.0-0.999)/(1-0.999)) = ceil(1001) → clamp to 4
    // freq_min = 2000 * (2.0 + 3*0.999)/4 = 2000 * 4.997/4 = 2498.5 → min(2498.5,2000)=2000
    // 2000 >= freq_eff(1000) → ceil_to_mode(2000)=2000, all 4 cores
    auto target = policy.compute_target(2.0, 0.999, 4, *clock_domain_);
    EXPECT_DOUBLE_EQ(target.frequency.mhz, 2000.0);
    EXPECT_EQ(target.active_processors, 4u);
}

TEST_F(CsfPolicyTest, ComputeTarget_TwoStageReduction) {
    engine_.platform().finalize();
    TestableCsfPolicy policy(engine_);

    // U_active=0.3, U_max=0.15, m=4
    // m_min = ceil((0.3-0.15)/(1-0.15)) = ceil(0.1765) = 1
    // freq_min = 2000 * (0.3 + 0*0.15)/1 = 600
    // 600 < freq_eff(1000) → freq_eff, ceil(1*600/1000)=ceil(0.6)=1
    auto target = policy.compute_target(0.3, 0.15, 4, *clock_domain_);
    EXPECT_DOUBLE_EQ(target.frequency.mhz, 1000.0);
    EXPECT_EQ(target.active_processors, 1u);
}

TEST_F(CsfPolicyTest, ComputeTarget_SingleCore) {
    // Create a single-processor clock domain for this test
    Engine engine1;
    auto& pt = engine1.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine1.platform().add_clock_domain(Frequency{200.0}, Frequency{2000.0});
    cd.set_frequency_modes({
        Frequency{200.0}, Frequency{500.0}, Frequency{800.0},
        Frequency{1000.0}, Frequency{1500.0}, Frequency{2000.0}
    });
    cd.set_freq_eff(Frequency{1000.0});
    auto& pd = engine1.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}},
        {1, CStateScope::PerProcessor, Duration{0.001}, Power{10.0}}
    });
    engine1.platform().add_processor(pt, cd, pd);
    engine1.platform().finalize();

    TestableCsfPolicy policy(engine1);

    // U_active=0.5, U_max=0.5, m=1
    // max_util < 1.0 → m_min = ceil((0.5-0.5)/(1-0.5)) = ceil(0) = 0 → clamp to 1
    // freq_min = 2000 * (0.5 + 0*0.5)/1 = 1000
    // 1000 >= freq_eff(1000) → ceil_to_mode(1000)=1000, active=total=1
    auto target = policy.compute_target(0.5, 0.5, 1, cd);
    EXPECT_DOUBLE_EQ(target.frequency.mhz, 1000.0);
    EXPECT_EQ(target.active_processors, 1u);
}

TEST_F(CsfPolicyTest, ComputeTarget_ActiveUtilBelowMaxUtil) {
    engine_.platform().finalize();
    TestableCsfPolicy policy(engine_);

    // Degenerate case: one active server (U_active=0.1) but max_server_utilization()
    // returns a higher value from an inactive server (U_max=0.3).
    // m_min = ceil((0.1-0.3)/(1-0.3)) = ceil(-0.2857) = 0 → clamp to 1
    // freq_min = 2000 * (0.1 + 0*0.3)/1 = 200
    // 200 < freq_eff(1000) → freq_eff, ceil(1*200/1000) = ceil(0.2) = 1
    auto target = policy.compute_target(0.1, 0.3, 4, *clock_domain_);
    EXPECT_DOUBLE_EQ(target.frequency.mhz, 1000.0);
    EXPECT_EQ(target.active_processors, 1u);
}

// ---------------------------------------------------------------------------
// Comparative test: FFA vs CSF on same workload
// ---------------------------------------------------------------------------

TEST_F(CsfPolicyTest, FfaVsCsf_SameWorkload) {
    engine_.platform().finalize();

    TestableFfaPolicyForComparison ffa_policy(engine_);
    TestableCsfPolicy csf_policy(engine_);

    // U_active=0.6, U_max=0.3, m=4
    // FFA: freq_min = 2000 * (0.6 + 3*0.3)/4 = 2000 * 1.5/4 = 750
    //   750 < freq_eff(1000) → freq_eff, ceil(4*750/1000) = ceil(3.0) = 3
    //   → (1000 MHz, 3 cores)
    auto ffa_target = ffa_policy.compute_target(0.6, 0.3, 4, *clock_domain_);
    EXPECT_DOUBLE_EQ(ffa_target.frequency.mhz, 1000.0);
    EXPECT_EQ(ffa_target.active_processors, 3u);

    // CSF: m_min = ceil((0.6-0.3)/(1-0.3)) = ceil(0.4286) = 1
    //   freq_min = 2000 * (0.6 + 0*0.3)/1 = 1200
    //   1200 >= freq_eff(1000) → else branch, active=total=4
    //   ceil_to_mode(1200) = 1500
    //   → (1500 MHz, 4 cores)
    auto csf_target = csf_policy.compute_target(0.6, 0.3, 4, *clock_domain_);
    EXPECT_DOUBLE_EQ(csf_target.frequency.mhz, 1500.0);
    EXPECT_EQ(csf_target.active_processors, 4u);

    // CSF trades more cores for higher frequency in this regime
    EXPECT_GT(csf_target.active_processors, ffa_target.active_processors);
    EXPECT_GT(csf_target.frequency.mhz, ffa_target.frequency.mhz);
}
