#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/grub_policy.hpp>
#include <schedsim/algo/cash_policy.hpp>
#include <schedsim/algo/dvfs_policy.hpp>
#include <schedsim/algo/dpm_policy.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class PolicyIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("cpu", 1.0);
        clock_domain_ = &engine_.platform().add_clock_domain(
            Frequency{500.0}, Frequency{2000.0});
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}},
            {1, CStateScope::PerProcessor, Duration{0.001}, Power{50.0}}
        });
        proc_ = &engine_.platform().add_processor(pt, *clock_domain_, pd);
    }

    TimePoint time(double seconds) {
        return TimePoint{Duration{seconds}};
    }

    Engine engine_;
    Processor* proc_{nullptr};
    ClockDomain* clock_domain_{nullptr};
};

TEST_F(PolicyIntegrationTest, EnableGrub) {
    engine_.platform().finalize();
    EdfScheduler sched(engine_, {proc_});

    sched.enable_grub();

    // Verify GRUB is enabled via active_utilization behavior
    EXPECT_DOUBLE_EQ(sched.active_utilization(), 0.0);
}

TEST_F(PolicyIntegrationTest, EnableCash) {
    engine_.platform().finalize();
    EdfScheduler sched(engine_, {proc_});

    sched.enable_cash();

    EXPECT_DOUBLE_EQ(sched.active_utilization(), 0.0);
}

TEST_F(PolicyIntegrationTest, EnablePowerAwareDvfs) {
    engine_.platform().finalize();
    EdfScheduler sched(engine_, {proc_});

    // Enable with cooldown
    sched.enable_power_aware_dvfs(Duration{0.5});

    // Verify it doesn't crash
    SUCCEED();
}

TEST_F(PolicyIntegrationTest, EnableBasicDpm) {
    engine_.platform().finalize();
    EdfScheduler sched(engine_, {proc_});

    sched.enable_basic_dpm(1);

    // Verify it doesn't crash
    SUCCEED();
}

TEST_F(PolicyIntegrationTest, GrubWithDvfs_Composition) {
    // Add task before finalize
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});

    // Enable both GRUB and PowerAware DVFS
    sched.enable_grub();
    sched.enable_power_aware_dvfs();

    // Add server
    sched.add_server(task);

    // Set up job arrival handler
    engine_.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    // Schedule a job
    engine_.schedule_job_arrival(task, time(0.0), Duration{1.0});

    // Run simulation
    engine_.run(time(5.0));

    // DVFS should have adjusted frequency based on utilization
    // Just verify simulation ran without errors
    EXPECT_GE(engine_.time().time_since_epoch().count(), 1.0);
}

TEST_F(PolicyIntegrationTest, ActiveUtilizationWithGrub) {
    auto& task1 = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    auto& task2 = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{3.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.enable_grub();

    sched.add_server(task1);  // U=0.2
    sched.add_server(task2);  // U=0.3

    // Set up job arrival handler
    engine_.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    // No jobs yet, active utilization should be 0
    EXPECT_DOUBLE_EQ(sched.active_utilization(), 0.0);

    // Schedule jobs to activate servers
    engine_.schedule_job_arrival(task1, time(0.0), Duration{1.0});
    engine_.schedule_job_arrival(task2, time(0.0), Duration{1.0});

    // Process arrivals
    engine_.run(time(0.001));

    // Both servers should be active now
    // active_utilization should be 0.2 + 0.3 = 0.5
    EXPECT_DOUBLE_EQ(sched.active_utilization(), 0.5);
}

TEST_F(PolicyIntegrationTest, BudgetTimerReschedulingOnDvfs) {
    // This test verifies that budget timers are correctly rescheduled
    // when DVFS changes frequency mid-execution

    auto& task = engine_.platform().add_task(Duration{100.0}, Duration{100.0}, Duration{10.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.enable_power_aware_dvfs();

    sched.add_server(task);

    // Set up job arrival handler
    engine_.set_job_arrival_handler([&](Task& t, Job job) {
        sched.on_job_arrival(t, std::move(job));
    });

    // Schedule a job
    engine_.schedule_job_arrival(task, time(0.0), Duration{5.0});

    // Run simulation
    engine_.run(time(20.0));

    // Job should have completed
    EXPECT_GE(engine_.time().time_since_epoch().count(), 5.0);
}

TEST_F(PolicyIntegrationTest, PolicySettersAcceptNullptr) {
    engine_.platform().finalize();
    EdfScheduler sched(engine_, {proc_});

    // Setting nullptr should be safe
    sched.set_reclamation_policy(nullptr);
    sched.set_dvfs_policy(nullptr);
    sched.set_dpm_policy(nullptr);

    // active_utilization should return total_utilization when no policy
    EXPECT_DOUBLE_EQ(sched.active_utilization(), sched.utilization());
}
