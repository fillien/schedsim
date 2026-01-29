#include <schedsim/io/scenario_generation.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <numeric>

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
