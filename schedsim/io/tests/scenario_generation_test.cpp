#include <schedsim/io/scenario_generation.hpp>
#include <schedsim/io/scenario_loader.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <numeric>
#include <sstream>

using namespace schedsim::io;
using namespace schedsim::core;

class ScenarioGenerationTest : public ::testing::Test {
protected:
    std::mt19937 rng{42};  // Fixed seed for reproducibility
};

// =============================================================================
// UUniFast Tests
// =============================================================================

TEST_F(ScenarioGenerationTest, UUniFastSumsToTarget) {
    double target = 0.8;
    auto utils = uunifast(5, target, rng);

    double sum = std::accumulate(utils.begin(), utils.end(), 0.0);
    EXPECT_NEAR(sum, target, 1e-10);
}

TEST_F(ScenarioGenerationTest, UUniFastAllPositive) {
    double target = 0.7;
    auto utils = uunifast(10, target, rng);

    for (double util : utils) {
        EXPECT_GE(util, 0.0);
        EXPECT_LE(util, target);
    }
}

TEST_F(ScenarioGenerationTest, UUniFastSingleTask) {
    double target = 0.5;
    auto utils = uunifast(1, target, rng);

    ASSERT_EQ(utils.size(), 1u);
    EXPECT_DOUBLE_EQ(utils[0], target);
}

TEST_F(ScenarioGenerationTest, UUniFastZeroTasks) {
    auto utils = uunifast(0, 0.5, rng);
    EXPECT_TRUE(utils.empty());
}

TEST_F(ScenarioGenerationTest, UUniFastFullUtilization) {
    double target = 1.0;
    auto utils = uunifast(4, target, rng);

    double sum = std::accumulate(utils.begin(), utils.end(), 0.0);
    EXPECT_NEAR(sum, target, 1e-10);
}

TEST_F(ScenarioGenerationTest, UUniFastDifferentSeeds) {
    std::mt19937 rng1{100};
    std::mt19937 rng2{200};

    auto utils1 = uunifast(5, 0.5, rng1);
    auto utils2 = uunifast(5, 0.5, rng2);

    // Different seeds should produce different results
    bool different = false;
    for (std::size_t idx = 0; idx < utils1.size(); ++idx) {
        if (std::abs(utils1[idx] - utils2[idx]) > 1e-10) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

// =============================================================================
// Task Set Generation Tests
// =============================================================================

TEST_F(ScenarioGenerationTest, GenerateTaskSetBasic) {
    PeriodDistribution period_dist{
        .min = Duration{10.0},
        .max = Duration{100.0},
        .log_uniform = true
    };

    auto tasks = generate_task_set(5, 0.6, period_dist, rng);

    EXPECT_EQ(tasks.size(), 5u);

    // Check that all periods are in range
    for (const auto& task : tasks) {
        EXPECT_GE(task.period.count(), 10.0);
        EXPECT_LE(task.period.count(), 100.0);
        EXPECT_GT(task.wcet.count(), 0.0);
        EXPECT_LE(task.wcet.count(), task.period.count());
        // Deadline should equal period (implicit deadline)
        EXPECT_DOUBLE_EQ(task.relative_deadline.count(), task.period.count());
    }
}

TEST_F(ScenarioGenerationTest, GenerateTaskSetUniformPeriods) {
    PeriodDistribution period_dist{
        .min = Duration{50.0},
        .max = Duration{50.0},  // Same min and max
        .log_uniform = false
    };

    auto tasks = generate_task_set(3, 0.5, period_dist, rng);

    for (const auto& task : tasks) {
        EXPECT_DOUBLE_EQ(task.period.count(), 50.0);
    }
}

TEST_F(ScenarioGenerationTest, GenerateTaskSetEmptyOnZero) {
    PeriodDistribution period_dist{Duration{10.0}, Duration{100.0}, true};

    auto tasks = generate_task_set(0, 0.5, period_dist, rng);
    EXPECT_TRUE(tasks.empty());
}

TEST_F(ScenarioGenerationTest, GenerateTaskSetUtilizationSums) {
    PeriodDistribution period_dist{Duration{10.0}, Duration{100.0}, true};
    double target = 0.7;

    auto tasks = generate_task_set(8, target, period_dist, rng);

    double total_util = 0.0;
    for (const auto& task : tasks) {
        total_util += task.wcet.count() / task.period.count();
    }
    EXPECT_NEAR(total_util, target, 1e-10);
}

// =============================================================================
// Arrival Generation Tests
// =============================================================================

TEST_F(ScenarioGenerationTest, GenerateArrivalsBasic) {
    std::vector<TaskParams> tasks;
    tasks.push_back(TaskParams{
        .id = 0,
        .period = Duration{10.0},
        .relative_deadline = Duration{10.0},
        .wcet = Duration{2.0},
        .jobs = {}
    });

    generate_arrivals(tasks, Duration{50.0}, rng, 1.0);

    // Should have arrivals at t=0, 10, 20, 30, 40
    ASSERT_EQ(tasks[0].jobs.size(), 5u);
    EXPECT_DOUBLE_EQ(tasks[0].jobs[0].arrival.time_since_epoch().count(), 0.0);
    EXPECT_DOUBLE_EQ(tasks[0].jobs[1].arrival.time_since_epoch().count(), 10.0);
    EXPECT_DOUBLE_EQ(tasks[0].jobs[2].arrival.time_since_epoch().count(), 20.0);
    EXPECT_DOUBLE_EQ(tasks[0].jobs[3].arrival.time_since_epoch().count(), 30.0);
    EXPECT_DOUBLE_EQ(tasks[0].jobs[4].arrival.time_since_epoch().count(), 40.0);
}

TEST_F(ScenarioGenerationTest, GenerateArrivalsWorstCase) {
    std::vector<TaskParams> tasks;
    tasks.push_back(TaskParams{
        .id = 0,
        .period = Duration{10.0},
        .relative_deadline = Duration{10.0},
        .wcet = Duration{3.0},
        .jobs = {}
    });

    generate_arrivals(tasks, Duration{20.0}, rng, 1.0);  // exec_ratio = 1.0

    for (const auto& job : tasks[0].jobs) {
        EXPECT_DOUBLE_EQ(job.duration.count(), 3.0);  // All at WCET
    }
}

TEST_F(ScenarioGenerationTest, GenerateArrivalsPartialExec) {
    std::vector<TaskParams> tasks;
    tasks.push_back(TaskParams{
        .id = 0,
        .period = Duration{10.0},
        .relative_deadline = Duration{10.0},
        .wcet = Duration{4.0},
        .jobs = {}
    });

    generate_arrivals(tasks, Duration{100.0}, rng, 0.5);  // exec_ratio = 0.5

    // All durations should be <= 0.5 * wcet = 2.0
    for (const auto& job : tasks[0].jobs) {
        EXPECT_LE(job.duration.count(), 2.0 + 1e-10);
        EXPECT_GT(job.duration.count(), 0.0);
    }
}

TEST_F(ScenarioGenerationTest, GenerateArrivalsMultipleTasks) {
    std::vector<TaskParams> tasks;
    tasks.push_back(TaskParams{0, Duration{10.0}, Duration{10.0}, Duration{2.0}, {}});
    tasks.push_back(TaskParams{1, Duration{20.0}, Duration{20.0}, Duration{4.0}, {}});

    generate_arrivals(tasks, Duration{40.0}, rng);

    // Task 0: arrivals at 0, 10, 20, 30 (4 jobs)
    EXPECT_EQ(tasks[0].jobs.size(), 4u);
    // Task 1: arrivals at 0, 20 (2 jobs)
    EXPECT_EQ(tasks[1].jobs.size(), 2u);
}

// =============================================================================
// Complete Scenario Generation Tests
// =============================================================================

TEST_F(ScenarioGenerationTest, GenerateScenarioComplete) {
    PeriodDistribution period_dist{Duration{10.0}, Duration{50.0}, true};

    auto scenario = generate_scenario(4, 0.6, period_dist, Duration{100.0}, rng);

    EXPECT_EQ(scenario.tasks.size(), 4u);

    for (const auto& task : scenario.tasks) {
        EXPECT_FALSE(task.jobs.empty());
        EXPECT_GE(task.period.count(), 10.0);
        EXPECT_LE(task.period.count(), 50.0);
    }
}

TEST_F(ScenarioGenerationTest, GenerateScenarioZeroTasks) {
    PeriodDistribution period_dist{Duration{10.0}, Duration{100.0}, true};

    auto scenario = generate_scenario(0, 0.5, period_dist, Duration{100.0}, rng);

    EXPECT_TRUE(scenario.tasks.empty());
}

// =============================================================================
// UUniFast-Discard Tests
// =============================================================================

TEST_F(ScenarioGenerationTest, UUniFastDiscardSumsToTarget) {
    double target = 2.5;
    double umin = 0.1;
    double umax = 0.8;

    auto utils = uunifast_discard(5, target, umin, umax, rng);

    double sum = std::accumulate(utils.begin(), utils.end(), 0.0);
    EXPECT_NEAR(sum, target, 1e-6);
}

TEST_F(ScenarioGenerationTest, UUniFastDiscardRespectsUminUmax) {
    double target = 3.0;
    double umin = 0.2;
    double umax = 0.9;

    auto utils = uunifast_discard(5, target, umin, umax, rng);

    EXPECT_EQ(utils.size(), 5u);
    for (double util : utils) {
        EXPECT_GE(util, umin);
        EXPECT_LE(util, umax);
    }
}

TEST_F(ScenarioGenerationTest, UUniFastDiscardMulticoreUtilization) {
    // Test utilization > 1.0 (multicore scenario)
    double target = 3.5;
    double umin = 0.3;
    double umax = 1.0;

    auto utils = uunifast_discard(5, target, umin, umax, rng);

    double sum = std::accumulate(utils.begin(), utils.end(), 0.0);
    EXPECT_NEAR(sum, target, 1e-6);

    for (double util : utils) {
        EXPECT_GE(util, umin);
        EXPECT_LE(util, umax);
    }
}

TEST_F(ScenarioGenerationTest, UUniFastDiscardZeroTasks) {
    auto utils = uunifast_discard(0, 0.5, 0.0, 1.0, rng);
    EXPECT_TRUE(utils.empty());
}

// =============================================================================
// Harmonic Period Tests
// =============================================================================

TEST_F(ScenarioGenerationTest, HarmonicPeriodsAreUsed) {
    // Generate many periods and verify they're all from the harmonic set
    for (int i = 0; i < 100; ++i) {
        Duration period = pick_harmonic_period(rng);
        int period_us = static_cast<int>(period.count() * 1'000'000.0);

        // Check that the period divides the hyperperiod
        EXPECT_EQ(HYPERPERIOD_US % period_us, 0)
            << "Period " << period_us << " does not divide hyperperiod";

        // Check that the period is in the valid set
        bool found = false;
        for (int valid_period : HARMONIC_PERIODS_US) {
            if (period_us == valid_period) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Period " << period_us << " not in harmonic set";
    }
}

// =============================================================================
// Weibull Job Generation Tests
// =============================================================================

TEST_F(ScenarioGenerationTest, WeibullJobsDontExceedWcet) {
    Duration period{0.01};  // 10ms
    Duration wcet{0.002};   // 2ms
    WeibullJobConfig config{.success_rate = 0.5, .compression_rate = 0.5};

    auto jobs = generate_weibull_jobs(period, wcet, 10, config, rng);

    EXPECT_EQ(jobs.size(), 10u);
    for (const auto& job : jobs) {
        EXPECT_LE(job.duration.count(), wcet.count() + 1e-10);
    }
}

TEST_F(ScenarioGenerationTest, WeibullJobsRespectCompression) {
    Duration period{0.01};
    Duration wcet{0.002};
    double compression = 0.4;
    WeibullJobConfig config{.success_rate = 1.0, .compression_rate = compression};

    auto jobs = generate_weibull_jobs(period, wcet, 20, config, rng);

    double min_expected = compression * wcet.count();
    for (const auto& job : jobs) {
        EXPECT_GE(job.duration.count(), min_expected - 1e-10);
        EXPECT_LE(job.duration.count(), wcet.count() + 1e-10);
    }
}

TEST_F(ScenarioGenerationTest, WeibullJobsNoCompressionExactWcet) {
    Duration period{0.01};
    Duration wcet{0.002};
    WeibullJobConfig config{.success_rate = 1.0, .compression_rate = 1.0};

    auto jobs = generate_weibull_jobs(period, wcet, 10, config, rng);

    for (const auto& job : jobs) {
        EXPECT_DOUBLE_EQ(job.duration.count(), wcet.count());
    }
}

TEST_F(ScenarioGenerationTest, WeibullJobsSequentialArrivals) {
    Duration period{0.005};  // 5ms
    Duration wcet{0.001};
    WeibullJobConfig config{.success_rate = 1.0, .compression_rate = 1.0};

    auto jobs = generate_weibull_jobs(period, wcet, 5, config, rng);

    ASSERT_EQ(jobs.size(), 5u);
    for (std::size_t i = 0; i < jobs.size(); ++i) {
        double expected_arrival = static_cast<double>(i) * period.count();
        EXPECT_DOUBLE_EQ(jobs[i].arrival.time_since_epoch().count(), expected_arrival);
    }
}

TEST_F(ScenarioGenerationTest, WeibullJobsZeroJobs) {
    Duration period{0.01};
    Duration wcet{0.002};
    WeibullJobConfig config{};

    auto jobs = generate_weibull_jobs(period, wcet, 0, config, rng);
    EXPECT_TRUE(jobs.empty());
}

// =============================================================================
// Full UUniFast-Discard-Weibull Generation Tests
// =============================================================================

TEST_F(ScenarioGenerationTest, UUniFastDiscardWeibullBasic) {
    WeibullJobConfig config{.success_rate = 0.9, .compression_rate = 0.5};

    auto scenario = generate_uunifast_discard_weibull(5, 2.5, 0.1, 0.9, config, rng);

    EXPECT_EQ(scenario.tasks.size(), 5u);

    double total_util = 0.0;
    for (const auto& task : scenario.tasks) {
        total_util += task.wcet.count() / task.period.count();

        // Verify task has jobs
        EXPECT_FALSE(task.jobs.empty());

        // Verify deadline equals period
        EXPECT_DOUBLE_EQ(task.relative_deadline.count(), task.period.count());

        // Verify jobs don't exceed WCET
        for (const auto& job : task.jobs) {
            EXPECT_LE(job.duration.count(), task.wcet.count() + 1e-10);
        }
    }

    EXPECT_NEAR(total_util, 2.5, 0.01);
}

TEST_F(ScenarioGenerationTest, TaskIdsStartAtOne) {
    WeibullJobConfig config{};

    auto scenario = generate_uunifast_discard_weibull(3, 1.5, 0.0, 1.0, config, rng);

    ASSERT_EQ(scenario.tasks.size(), 3u);
    EXPECT_EQ(scenario.tasks[0].id, 1u);
    EXPECT_EQ(scenario.tasks[1].id, 2u);
    EXPECT_EQ(scenario.tasks[2].id, 3u);
}

TEST_F(ScenarioGenerationTest, UUniFastDiscardWeibullZeroTasks) {
    WeibullJobConfig config{};

    auto scenario = generate_uunifast_discard_weibull(0, 0.0, 0.0, 1.0, config, rng);

    EXPECT_TRUE(scenario.tasks.empty());
}

TEST_F(ScenarioGenerationTest, UUniFastDiscardWeibullThrowsOnInvalidUmin) {
    WeibullJobConfig config{};

    EXPECT_THROW(
        generate_uunifast_discard_weibull(5, 3.0, -0.1, 1.0, config, rng),
        std::invalid_argument
    );
}

TEST_F(ScenarioGenerationTest, UUniFastDiscardWeibullThrowsOnInvalidUmax) {
    WeibullJobConfig config{};

    EXPECT_THROW(
        generate_uunifast_discard_weibull(5, 3.0, 0.0, 1.5, config, rng),
        std::invalid_argument
    );
}

TEST_F(ScenarioGenerationTest, UUniFastDiscardWeibullThrowsOnInfeasible) {
    WeibullJobConfig config{};

    // num_tasks * umax = 5 * 0.5 = 2.5 < target 3.0
    EXPECT_THROW(
        generate_uunifast_discard_weibull(5, 3.0, 0.0, 0.5, config, rng),
        std::invalid_argument
    );

    // num_tasks * umin = 5 * 0.7 = 3.5 > target 3.0
    EXPECT_THROW(
        generate_uunifast_discard_weibull(5, 3.0, 0.7, 1.0, config, rng),
        std::invalid_argument
    );
}

// =============================================================================
// Merge Scenarios Tests
// =============================================================================

TEST_F(ScenarioGenerationTest, MergeScenariosReassignsIds) {
    ScenarioData scenario_a;
    scenario_a.tasks.push_back(TaskParams{
        .id = 100,  // Arbitrary ID
        .period = Duration{0.01},
        .relative_deadline = Duration{0.01},
        .wcet = Duration{0.001},
        .jobs = {}
    });
    scenario_a.tasks.push_back(TaskParams{
        .id = 200,
        .period = Duration{0.02},
        .relative_deadline = Duration{0.02},
        .wcet = Duration{0.002},
        .jobs = {}
    });

    ScenarioData scenario_b;
    scenario_b.tasks.push_back(TaskParams{
        .id = 300,
        .period = Duration{0.03},
        .relative_deadline = Duration{0.03},
        .wcet = Duration{0.003},
        .jobs = {}
    });

    auto merged = merge_scenarios(scenario_a, scenario_b);

    ASSERT_EQ(merged.tasks.size(), 3u);
    EXPECT_EQ(merged.tasks[0].id, 1u);
    EXPECT_EQ(merged.tasks[1].id, 2u);
    EXPECT_EQ(merged.tasks[2].id, 3u);

    // Verify task properties preserved
    EXPECT_DOUBLE_EQ(merged.tasks[0].period.count(), 0.01);
    EXPECT_DOUBLE_EQ(merged.tasks[1].period.count(), 0.02);
    EXPECT_DOUBLE_EQ(merged.tasks[2].period.count(), 0.03);
}

TEST_F(ScenarioGenerationTest, MergeScenariosEmptyFirst) {
    ScenarioData scenario_a;
    ScenarioData scenario_b;
    scenario_b.tasks.push_back(TaskParams{
        .id = 99,
        .period = Duration{0.01},
        .relative_deadline = Duration{0.01},
        .wcet = Duration{0.001},
        .jobs = {}
    });

    auto merged = merge_scenarios(scenario_a, scenario_b);

    ASSERT_EQ(merged.tasks.size(), 1u);
    EXPECT_EQ(merged.tasks[0].id, 1u);
}

TEST_F(ScenarioGenerationTest, MergeScenariosBothEmpty) {
    ScenarioData scenario_a;
    ScenarioData scenario_b;

    auto merged = merge_scenarios(scenario_a, scenario_b);

    EXPECT_TRUE(merged.tasks.empty());
}

// =============================================================================
// From Utilizations Tests
// =============================================================================

TEST_F(ScenarioGenerationTest, FromUtilizationsBuildsCorrectTasks) {
    std::vector<double> utilizations{0.3, 0.4, 0.2};
    WeibullJobConfig config{.success_rate = 1.0, .compression_rate = 1.0};

    auto tasks = from_utilizations(utilizations, config, rng);

    ASSERT_EQ(tasks.size(), 3u);

    for (std::size_t i = 0; i < tasks.size(); ++i) {
        // Task IDs start at 1
        EXPECT_EQ(tasks[i].id, i + 1);

        // WCET/period should match requested utilization
        double actual_util = tasks[i].wcet.count() / tasks[i].period.count();
        EXPECT_NEAR(actual_util, utilizations[i], 1e-10);

        // Should have jobs
        EXPECT_FALSE(tasks[i].jobs.empty());
    }
}

TEST_F(ScenarioGenerationTest, FromUtilizationsThrowsOnInvalidUtil) {
    std::vector<double> utilizations{0.3, 1.5, 0.2};  // 1.5 > 1.0
    WeibullJobConfig config{};

    EXPECT_THROW(from_utilizations(utilizations, config, rng), std::invalid_argument);
}

TEST_F(ScenarioGenerationTest, FromUtilizationsEmpty) {
    std::vector<double> utilizations;
    WeibullJobConfig config{};

    auto tasks = from_utilizations(utilizations, config, rng);
    EXPECT_TRUE(tasks.empty());
}

// =============================================================================
// Round-Trip Test (Generate and Load)
// =============================================================================

TEST_F(ScenarioGenerationTest, GeneratedScenarioLoadsCorrectly) {
    // This test requires scenario_loader to be able to parse the output
    // of generate_uunifast_discard_weibull
    WeibullJobConfig config{.success_rate = 0.8, .compression_rate = 0.6};

    auto original = generate_uunifast_discard_weibull(3, 1.5, 0.2, 0.8, config, rng);

    // Write to string stream
    std::ostringstream oss;
    write_scenario_to_stream(original, oss);

    // Load from string
    auto loaded = load_scenario_from_string(oss.str());

    // Verify round-trip
    ASSERT_EQ(loaded.tasks.size(), original.tasks.size());

    for (std::size_t i = 0; i < original.tasks.size(); ++i) {
        const auto& orig_task = original.tasks[i];
        const auto& loaded_task = loaded.tasks[i];

        EXPECT_EQ(loaded_task.id, orig_task.id);
        EXPECT_NEAR(loaded_task.period.count(), orig_task.period.count(), 1e-9);
        EXPECT_NEAR(loaded_task.relative_deadline.count(), orig_task.relative_deadline.count(), 1e-9);
        EXPECT_NEAR(loaded_task.wcet.count(), orig_task.wcet.count(), 1e-9);
        EXPECT_EQ(loaded_task.jobs.size(), orig_task.jobs.size());
    }
}
