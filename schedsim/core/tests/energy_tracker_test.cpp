#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

class EnergyTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("big", 1.0);
        cd_ = &engine_.platform().add_clock_domain(Frequency{1000.0}, Frequency{2000.0});
        pd_ = &engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}},
            {1, CStateScope::PerProcessor, Duration{0.01}, Power{10.0}}
        });
        proc_ = &engine_.platform().add_processor(pt, *cd_, *pd_);
        engine_.platform().finalize();

        // Set power coefficients: P(f) = 50 + 100*f mW (f in GHz)
        // At 2 GHz: P = 50 + 200 = 250 mW
        // At 1 GHz: P = 50 + 100 = 150 mW
        cd_->set_power_coefficients({50.0, 100.0, 0.0, 0.0});

        task_ = std::make_unique<Task>(0, Duration{10.0}, Duration{10.0}, Duration{2.0});
    }

    Engine engine_;
    ClockDomain* cd_{nullptr};
    PowerDomain* pd_{nullptr};
    Processor* proc_{nullptr};
    std::unique_ptr<Task> task_;
};

TEST_F(EnergyTrackerTest, DisabledByDefault) {
    EXPECT_FALSE(engine_.energy_tracking_enabled());
}

TEST_F(EnergyTrackerTest, EnableDisable) {
    engine_.enable_energy_tracking(true);
    EXPECT_TRUE(engine_.energy_tracking_enabled());

    engine_.enable_energy_tracking(false);
    EXPECT_FALSE(engine_.energy_tracking_enabled());
}

TEST_F(EnergyTrackerTest, QueriesThrowWhenDisabled) {
    EXPECT_THROW(engine_.processor_energy(0), InvalidStateError);
    EXPECT_THROW(engine_.clock_domain_energy(0), InvalidStateError);
    EXPECT_THROW(engine_.power_domain_energy(0), InvalidStateError);
    EXPECT_THROW(engine_.total_energy(), InvalidStateError);
}

TEST_F(EnergyTrackerTest, ZeroEnergyAtStart) {
    engine_.enable_energy_tracking(true);

    EXPECT_DOUBLE_EQ(engine_.processor_energy(0).mj, 0.0);
    EXPECT_DOUBLE_EQ(engine_.clock_domain_energy(0).mj, 0.0);
    EXPECT_DOUBLE_EQ(engine_.power_domain_energy(0).mj, 0.0);
    EXPECT_DOUBLE_EQ(engine_.total_energy().mj, 0.0);
}

TEST_F(EnergyTrackerTest, IdleEnergyAccumulation) {
    engine_.enable_energy_tracking(true);

    // Run for 1 second while idle
    // At 2 GHz (max freq), P = 50 + 100*2 = 250 mW
    // Energy = 250 mW * 1s = 250 mJ
    engine_.run(TimePoint{Duration{1.0}});

    Energy e = engine_.processor_energy(0);
    EXPECT_NEAR(e.mj, 250.0, 0.1);
}

TEST_F(EnergyTrackerTest, SleepEnergyAccumulation) {
    engine_.enable_energy_tracking(true);

    proc_->request_cstate(1);  // C1: 10 mW

    // Run for 1 second while sleeping
    // Energy = 10 mW * 1s = 10 mJ
    engine_.run(TimePoint{Duration{1.0}});

    Energy e = engine_.processor_energy(0);
    EXPECT_NEAR(e.mj, 10.0, 0.1);
}

TEST_F(EnergyTrackerTest, FrequencyChangeAffectsEnergy) {
    engine_.enable_energy_tracking(true);

    // Run for 0.5s at max freq (2 GHz)
    engine_.run(TimePoint{Duration{0.5}});

    // Change to half freq (1 GHz)
    cd_->set_frequency(Frequency{1000.0});

    // Run for another 0.5s at 1 GHz (no transition delay in this domain)
    engine_.run(TimePoint{Duration{1.0}});

    // Energy: 0.5s at 250 mW + 0.5s at 150 mW = 125 + 75 = 200 mJ
    Energy e = engine_.processor_energy(0);
    EXPECT_NEAR(e.mj, 200.0, 1.0);
}

TEST_F(EnergyTrackerTest, RunningEnergy) {
    engine_.enable_energy_tracking(true);

    TimePoint deadline{Duration{10.0}};
    Job job(*task_, Duration{2.0}, deadline);

    proc_->assign(job);

    // Run for 1s while running
    // At 2 GHz: P = 250 mW
    engine_.run(TimePoint{Duration{1.0}});

    Energy e = engine_.processor_energy(0);
    EXPECT_NEAR(e.mj, 250.0, 0.1);
}

TEST_F(EnergyTrackerTest, MultiProcessorEnergy) {
    // Create a new engine with multiple processors
    Engine engine2;
    auto& pt = engine2.platform().add_processor_type("big", 1.0);
    auto& cd = engine2.platform().add_clock_domain(Frequency{1000.0}, Frequency{2000.0});
    auto& pd = engine2.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}},
        {1, CStateScope::PerProcessor, Duration{0.01}, Power{10.0}}
    });
    auto& proc1 = engine2.platform().add_processor(pt, cd, pd);
    auto& proc2 = engine2.platform().add_processor(pt, cd, pd);
    engine2.platform().finalize();

    cd.set_power_coefficients({50.0, 100.0, 0.0, 0.0});
    engine2.enable_energy_tracking(true);

    // proc1 idle, proc2 sleeping
    proc2.request_cstate(1);

    // Run for 1 second
    engine2.run(TimePoint{Duration{1.0}});

    // proc1: 250 mW * 1s = 250 mJ
    // proc2: 10 mW * 1s = 10 mJ
    EXPECT_NEAR(engine2.processor_energy(0).mj, 250.0, 0.1);
    EXPECT_NEAR(engine2.processor_energy(1).mj, 10.0, 0.1);

    // Total: 260 mJ
    EXPECT_NEAR(engine2.total_energy().mj, 260.0, 0.2);

    // Clock domain energy = sum of processors in that domain
    EXPECT_NEAR(engine2.clock_domain_energy(0).mj, 260.0, 0.2);

    // Power domain energy = sum of processors in that domain
    EXPECT_NEAR(engine2.power_domain_energy(0).mj, 260.0, 0.2);
}

TEST_F(EnergyTrackerTest, InvalidProcessorId) {
    engine_.enable_energy_tracking(true);

    // Invalid processor ID returns zero energy
    EXPECT_DOUBLE_EQ(engine_.processor_energy(999).mj, 0.0);
}

TEST_F(EnergyTrackerTest, InvalidDomainId) {
    engine_.enable_energy_tracking(true);

    // Invalid domain IDs return zero energy
    EXPECT_DOUBLE_EQ(engine_.clock_domain_energy(999).mj, 0.0);
    EXPECT_DOUBLE_EQ(engine_.power_domain_energy(999).mj, 0.0);
}

TEST_F(EnergyTrackerTest, EnergyWithDVFSDelay) {
    // Create clock domain with DVFS delay
    Engine engine2;
    auto& pt = engine2.platform().add_processor_type("big", 1.0);
    auto& cd = engine2.platform().add_clock_domain(
        Frequency{500.0}, Frequency{2000.0}, Duration{0.1});
    auto& pd = engine2.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto& proc = engine2.platform().add_processor(pt, cd, pd);
    engine2.platform().finalize();

    cd.set_power_coefficients({50.0, 100.0, 0.0, 0.0});
    engine2.enable_energy_tracking(true);

    // Run for 0.5s at 2 GHz
    engine2.run(TimePoint{Duration{0.5}});

    // Start DVFS transition
    cd.set_frequency(Frequency{1000.0});
    EXPECT_EQ(proc.state(), ProcessorState::Changing);

    // During DVFS transition (0.1s), power is still at old frequency
    engine2.run(TimePoint{Duration{0.65}});

    // After transition, run for 0.35s at new frequency
    engine2.run(TimePoint{Duration{1.0}});

    // Energy: 0.5s at 250mW + 0.1s at 250mW (during change) + 0.4s at 150mW
    // = 125 + 25 + 60 = 210 mJ
    Energy e = engine2.processor_energy(0);
    EXPECT_NEAR(e.mj, 210.0, 5.0);
}
