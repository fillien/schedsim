#include <schedsim/algo/cbs_server.hpp>
#include <schedsim/algo/csf_policy.hpp>
#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/ffa_policy.hpp>
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

// ---------------------------------------------------------------------------
// Shared helper: build a 4-core platform with energy tracking
// ---------------------------------------------------------------------------

struct DvfsTestPlatform {
    Engine engine;
    Processor* procs[4]{};
    ClockDomain* clock_domain{nullptr};
    int deadline_misses{0};

    void build() {
        auto& pt = engine.platform().add_processor_type("cpu", 1.0);
        clock_domain = &engine.platform().add_clock_domain(
            Frequency{200.0}, Frequency{2000.0});
        clock_domain->set_frequency_modes({
            Frequency{200.0}, Frequency{500.0}, Frequency{800.0},
            Frequency{1000.0}, Frequency{1500.0}, Frequency{2000.0}
        });
        clock_domain->set_freq_eff(Frequency{1000.0});
        // P(f) = 50 + 100*f mW (f in GHz)
        // At 2 GHz: 250 mW, at 1 GHz: 150 mW
        clock_domain->set_power_coefficients({50.0, 100.0, 0.0, 0.0});

        auto& pd = engine.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
            {1, CStateScope::PerProcessor, duration_from_seconds(0.001), Power{10.0}}
        });

        for (int i = 0; i < 4; ++i) {
            procs[i] = &engine.platform().add_processor(pt, *clock_domain, pd);
            procs[i]->set_deadline_miss_handler(
                [this](Processor&, Job&) { ++deadline_misses; });
        }

        engine.enable_energy_tracking(true);
    }

    TimePoint time(double seconds) {
        return time_from_seconds(seconds);
    }

    std::vector<Processor*> proc_vec() {
        return {procs[0], procs[1], procs[2], procs[3]};
    }
};

// ---------------------------------------------------------------------------
// Shared helper: run GRUB-only baseline (no DVFS) and return total energy
// ---------------------------------------------------------------------------

static Energy run_grub_baseline() {
    DvfsTestPlatform plat;
    plat.build();

    auto& task1 = plat.engine.platform().add_task(
        duration_from_seconds(5.0), duration_from_seconds(5.0),
        duration_from_seconds(1.5));
    auto& task2 = plat.engine.platform().add_task(
        duration_from_seconds(5.0), duration_from_seconds(5.0),
        duration_from_seconds(1.5));
    plat.engine.platform().finalize();

    EdfScheduler sched(plat.engine, plat.proc_vec());
    sched.enable_grub();

    sched.add_server(task1, duration_from_seconds(1.5), duration_from_seconds(5.0));
    sched.add_server(task2, duration_from_seconds(1.5), duration_from_seconds(5.0));

    plat.engine.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    plat.engine.schedule_job_arrival(task1, plat.time(0.0), duration_from_seconds(1.0));
    plat.engine.schedule_job_arrival(task2, plat.time(0.0), duration_from_seconds(1.0));
    plat.engine.run(plat.time(5.0));

    return plat.engine.total_energy();
}

// ---------------------------------------------------------------------------
// PA integration tests
// ---------------------------------------------------------------------------

class DvfsPaIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        plat_.build();
    }
    DvfsTestPlatform plat_;
};

TEST_F(DvfsPaIntegrationTest, LowUtil_EnergySaving) {
    // 1 task U=0.1, run for 10s
    auto& task = plat_.engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0),
        duration_from_seconds(1.0));
    plat_.engine.platform().finalize();

    EdfScheduler sched(plat_.engine, plat_.proc_vec());
    sched.enable_grub();
    sched.enable_power_aware_dvfs();

    sched.add_server(task, duration_from_seconds(1.0), duration_from_seconds(10.0));

    plat_.engine.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    plat_.engine.schedule_job_arrival(task, plat_.time(0.0), duration_from_seconds(0.5));
    plat_.engine.run(plat_.time(10.0));

    // No deadline misses
    EXPECT_EQ(plat_.deadline_misses, 0);
    EXPECT_TRUE(sched.find_server(task)->state() == CbsServer::State::Inactive
             || sched.find_server(task)->state() == CbsServer::State::Ready);

    // Energy tracked and > 0
    Energy total = plat_.engine.total_energy();
    EXPECT_GT(total.mj, 0.0);

    // DVFS should have dropped frequency below f_max
    EXPECT_LT(plat_.clock_domain->frequency().mhz, 2000.0);
}

TEST_F(DvfsPaIntegrationTest, HighUtil_NearMaxFreq) {
    // 4 tasks U=0.9, run for 2s
    Task* tasks[4];
    for (int i = 0; i < 4; ++i) {
        tasks[i] = &plat_.engine.platform().add_task(
            duration_from_seconds(1.0), duration_from_seconds(1.0),
            duration_from_seconds(0.9));
    }
    plat_.engine.platform().finalize();

    EdfScheduler sched(plat_.engine, plat_.proc_vec());
    sched.enable_grub();
    sched.enable_power_aware_dvfs();

    for (int i = 0; i < 4; ++i) {
        sched.add_server(*tasks[i], duration_from_seconds(0.9), duration_from_seconds(1.0));
    }

    plat_.engine.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    for (int i = 0; i < 4; ++i) {
        plat_.engine.schedule_job_arrival(*tasks[i], plat_.time(0.0), duration_from_seconds(0.5));
    }
    plat_.engine.run(plat_.time(2.0));

    // At total U=3.6, PA formula yields freq >> f_max, so should be at 2000 MHz
    EXPECT_DOUBLE_EQ(plat_.clock_domain->frequency().mhz, 2000.0);
}

TEST_F(DvfsPaIntegrationTest, EnergyMonotonicity) {
    Energy energy_baseline = run_grub_baseline();

    // GRUB + PA DVFS (should save energy vs baseline)
    auto& task1 = plat_.engine.platform().add_task(
        duration_from_seconds(5.0), duration_from_seconds(5.0),
        duration_from_seconds(1.5));
    auto& task2 = plat_.engine.platform().add_task(
        duration_from_seconds(5.0), duration_from_seconds(5.0),
        duration_from_seconds(1.5));
    plat_.engine.platform().finalize();

    EdfScheduler sched(plat_.engine, plat_.proc_vec());
    sched.enable_grub();
    sched.enable_power_aware_dvfs();

    sched.add_server(task1, duration_from_seconds(1.5), duration_from_seconds(5.0));
    sched.add_server(task2, duration_from_seconds(1.5), duration_from_seconds(5.0));

    plat_.engine.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    plat_.engine.schedule_job_arrival(task1, plat_.time(0.0), duration_from_seconds(1.0));
    plat_.engine.schedule_job_arrival(task2, plat_.time(0.0), duration_from_seconds(1.0));
    plat_.engine.run(plat_.time(5.0));

    EXPECT_LT(plat_.engine.total_energy().mj, energy_baseline.mj);
}

// ---------------------------------------------------------------------------
// FFA integration tests
// ---------------------------------------------------------------------------

class DvfsFfaIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        plat_.build();
    }
    DvfsTestPlatform plat_;
};

TEST_F(DvfsFfaIntegrationTest, LowUtil_EnergySaving) {
    auto& task = plat_.engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0),
        duration_from_seconds(1.0));
    plat_.engine.platform().finalize();

    EdfScheduler sched(plat_.engine, plat_.proc_vec());
    sched.enable_grub();
    sched.enable_ffa();

    sched.add_server(task, duration_from_seconds(1.0), duration_from_seconds(10.0));

    plat_.engine.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    plat_.engine.schedule_job_arrival(task, plat_.time(0.0), duration_from_seconds(0.5));
    plat_.engine.run(plat_.time(10.0));

    EXPECT_EQ(plat_.deadline_misses, 0);
    EXPECT_TRUE(sched.find_server(task)->state() == CbsServer::State::Inactive
             || sched.find_server(task)->state() == CbsServer::State::Ready);

    Energy total = plat_.engine.total_energy();
    EXPECT_GT(total.mj, 0.0);

    // FFA should have dropped frequency (or put cores to sleep)
    EXPECT_LT(plat_.clock_domain->frequency().mhz, 2000.0);
}

TEST_F(DvfsFfaIntegrationTest, HighUtil_NearMaxFreq) {
    Task* tasks[4];
    for (int i = 0; i < 4; ++i) {
        tasks[i] = &plat_.engine.platform().add_task(
            duration_from_seconds(1.0), duration_from_seconds(1.0),
            duration_from_seconds(0.9));
    }
    plat_.engine.platform().finalize();

    EdfScheduler sched(plat_.engine, plat_.proc_vec());
    sched.enable_grub();
    sched.enable_ffa();

    for (int i = 0; i < 4; ++i) {
        sched.add_server(*tasks[i], duration_from_seconds(0.9), duration_from_seconds(1.0));
    }

    plat_.engine.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    for (int i = 0; i < 4; ++i) {
        plat_.engine.schedule_job_arrival(*tasks[i], plat_.time(0.0), duration_from_seconds(0.5));
    }
    // Check at 0.1s while all tasks are still active (FFA uses active_utilization)
    plat_.engine.run(plat_.time(0.1));

    // 4 tasks * U=0.9 → freq_min = 2000 * (3.6 + 3*0.9)/4 = 3150 → clamped to 2000
    EXPECT_DOUBLE_EQ(plat_.clock_domain->frequency().mhz, 2000.0);
}

TEST_F(DvfsFfaIntegrationTest, EnergyMonotonicity) {
    Energy energy_baseline = run_grub_baseline();

    // GRUB + FFA (should save energy vs baseline)
    auto& task1 = plat_.engine.platform().add_task(
        duration_from_seconds(5.0), duration_from_seconds(5.0),
        duration_from_seconds(1.5));
    auto& task2 = plat_.engine.platform().add_task(
        duration_from_seconds(5.0), duration_from_seconds(5.0),
        duration_from_seconds(1.5));
    plat_.engine.platform().finalize();

    EdfScheduler sched(plat_.engine, plat_.proc_vec());
    sched.enable_grub();
    sched.enable_ffa();

    sched.add_server(task1, duration_from_seconds(1.5), duration_from_seconds(5.0));
    sched.add_server(task2, duration_from_seconds(1.5), duration_from_seconds(5.0));

    plat_.engine.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    plat_.engine.schedule_job_arrival(task1, plat_.time(0.0), duration_from_seconds(1.0));
    plat_.engine.schedule_job_arrival(task2, plat_.time(0.0), duration_from_seconds(1.0));
    plat_.engine.run(plat_.time(5.0));

    EXPECT_LT(plat_.engine.total_energy().mj, energy_baseline.mj);
}

// ---------------------------------------------------------------------------
// CSF integration tests
// ---------------------------------------------------------------------------

class DvfsCsfIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        plat_.build();
    }
    DvfsTestPlatform plat_;
};

TEST_F(DvfsCsfIntegrationTest, LowUtil_EnergySaving) {
    auto& task = plat_.engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0),
        duration_from_seconds(1.0));
    plat_.engine.platform().finalize();

    EdfScheduler sched(plat_.engine, plat_.proc_vec());
    sched.enable_grub();
    sched.enable_csf();

    sched.add_server(task, duration_from_seconds(1.0), duration_from_seconds(10.0));

    plat_.engine.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    plat_.engine.schedule_job_arrival(task, plat_.time(0.0), duration_from_seconds(0.5));
    plat_.engine.run(plat_.time(10.0));

    EXPECT_EQ(plat_.deadline_misses, 0);
    EXPECT_TRUE(sched.find_server(task)->state() == CbsServer::State::Inactive
             || sched.find_server(task)->state() == CbsServer::State::Ready);

    Energy total = plat_.engine.total_energy();
    EXPECT_GT(total.mj, 0.0);

    EXPECT_LT(plat_.clock_domain->frequency().mhz, 2000.0);
}

TEST_F(DvfsCsfIntegrationTest, HighUtil_NearMaxFreq) {
    Task* tasks[4];
    for (int i = 0; i < 4; ++i) {
        tasks[i] = &plat_.engine.platform().add_task(
            duration_from_seconds(1.0), duration_from_seconds(1.0),
            duration_from_seconds(0.9));
    }
    plat_.engine.platform().finalize();

    EdfScheduler sched(plat_.engine, plat_.proc_vec());
    sched.enable_grub();
    sched.enable_csf();

    for (int i = 0; i < 4; ++i) {
        sched.add_server(*tasks[i], duration_from_seconds(0.9), duration_from_seconds(1.0));
    }

    plat_.engine.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    for (int i = 0; i < 4; ++i) {
        plat_.engine.schedule_job_arrival(*tasks[i], plat_.time(0.0), duration_from_seconds(0.5));
    }
    // Check at 0.1s while all tasks are still active (CSF uses active_utilization)
    plat_.engine.run(plat_.time(0.1));

    // 4 tasks * U=0.9, CSF: m_min = ceil((3.6-0.9)/(1-0.9)) = 27 → clamped to 4
    // freq_min = 2000 * (3.6 + 3*0.9)/4 = 3150 → clamped to 2000
    EXPECT_DOUBLE_EQ(plat_.clock_domain->frequency().mhz, 2000.0);
}

TEST_F(DvfsCsfIntegrationTest, EnergyMonotonicity) {
    Energy energy_baseline = run_grub_baseline();

    // GRUB + CSF (should save energy vs baseline)
    auto& task1 = plat_.engine.platform().add_task(
        duration_from_seconds(5.0), duration_from_seconds(5.0),
        duration_from_seconds(1.5));
    auto& task2 = plat_.engine.platform().add_task(
        duration_from_seconds(5.0), duration_from_seconds(5.0),
        duration_from_seconds(1.5));
    plat_.engine.platform().finalize();

    EdfScheduler sched(plat_.engine, plat_.proc_vec());
    sched.enable_grub();
    sched.enable_csf();

    sched.add_server(task1, duration_from_seconds(1.5), duration_from_seconds(5.0));
    sched.add_server(task2, duration_from_seconds(1.5), duration_from_seconds(5.0));

    plat_.engine.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    plat_.engine.schedule_job_arrival(task1, plat_.time(0.0), duration_from_seconds(1.0));
    plat_.engine.schedule_job_arrival(task2, plat_.time(0.0), duration_from_seconds(1.0));
    plat_.engine.run(plat_.time(5.0));

    EXPECT_LT(plat_.engine.total_energy().mj, energy_baseline.mj);
}

// ---------------------------------------------------------------------------
// Absolute energy value test
// ---------------------------------------------------------------------------

TEST(DvfsEnergyTest, AbsoluteEnergyValue) {
    // 1 core, frequency locked at 1000 MHz, idle for 1.0s
    // P(f) = 50 + 100*f mW (f in GHz) → P(1 GHz) = 150 mW
    // E = 150 mW * 1.0s = 150 mJ
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(
        Frequency{1000.0}, Frequency{1000.0});
    cd.set_power_coefficients({50.0, 100.0, 0.0, 0.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{0.0}}
    });
    engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    engine.enable_energy_tracking(true);

    // Add a dummy timer to advance time to 1.0s (no tasks needed)
    engine.add_timer(time_from_seconds(1.0), []() {});
    engine.run();

    Energy total = engine.total_energy();
    EXPECT_NEAR(total.mj, 150.0, 0.01);
}
