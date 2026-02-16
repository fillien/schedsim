#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/io/error.hpp>

#include <gtest/gtest.h>
#include <sstream>

using namespace schedsim::io;
using namespace schedsim::core;

class ScenarioLoaderTest : public ::testing::Test {};

// =============================================================================
// New Format Tests
// =============================================================================

TEST_F(ScenarioLoaderTest, LoadNewFormatBasic) {
    const char* json = R"({
        "tasks": [{
            "id": 0,
            "period": 10.0,
            "relative_deadline": 10.0,
            "wcet": 2.0,
            "jobs": [
                {"arrival": 0.0, "duration": 2.0},
                {"arrival": 10.0, "duration": 1.8}
            ]
        }]
    })";

    auto scenario = load_scenario_from_string(json);

    ASSERT_EQ(scenario.tasks.size(), 1u);
    const auto& task = scenario.tasks[0];
    EXPECT_EQ(task.id, 0u);
    EXPECT_DOUBLE_EQ(duration_to_seconds(task.period), 10.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(task.relative_deadline), 10.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(task.wcet), 2.0);

    ASSERT_EQ(task.jobs.size(), 2u);
    EXPECT_DOUBLE_EQ(time_to_seconds(task.jobs[0].arrival), 0.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(task.jobs[0].duration), 2.0);
    EXPECT_DOUBLE_EQ(time_to_seconds(task.jobs[1].arrival), 10.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(task.jobs[1].duration), 1.8);
}

TEST_F(ScenarioLoaderTest, LoadNewFormatDefaultDeadline) {
    const char* json = R"({
        "tasks": [{
            "id": 1,
            "period": 20.0,
            "wcet": 5.0
        }]
    })";

    auto scenario = load_scenario_from_string(json);

    ASSERT_EQ(scenario.tasks.size(), 1u);
    // Deadline should default to period
    EXPECT_DOUBLE_EQ(duration_to_seconds(scenario.tasks[0].relative_deadline), 20.0);
}

TEST_F(ScenarioLoaderTest, LoadNewFormatMultipleTasks) {
    const char* json = R"({
        "tasks": [
            {"id": 0, "period": 10.0, "wcet": 1.0},
            {"id": 1, "period": 20.0, "wcet": 2.0},
            {"id": 2, "period": 40.0, "wcet": 4.0}
        ]
    })";

    auto scenario = load_scenario_from_string(json);
    EXPECT_EQ(scenario.tasks.size(), 3u);
}

// =============================================================================
// Legacy Format Tests (utilization-based)
// =============================================================================

TEST_F(ScenarioLoaderTest, LoadLegacyFormatWithUtilization) {
    const char* json = R"({
        "tasks": [{
            "id": 1,
            "period": 100.0,
            "utilization": 0.2,
            "jobs": [
                {"arrival": 0.0, "duration": 18.0}
            ]
        }]
    })";

    auto scenario = load_scenario_from_string(json);

    ASSERT_EQ(scenario.tasks.size(), 1u);
    // WCET should be computed from utilization: period * utilization
    EXPECT_DOUBLE_EQ(duration_to_seconds(scenario.tasks[0].wcet), 20.0);
}

TEST_F(ScenarioLoaderTest, LoadLegacyFormatCoresIgnored) {
    const char* json = R"({
        "cores": 4,
        "tasks": [{
            "id": 0,
            "period": 10.0,
            "wcet": 2.0
        }]
    })";

    // Should not throw - "cores" field is ignored
    auto scenario = load_scenario_from_string(json);
    EXPECT_EQ(scenario.tasks.size(), 1u);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(ScenarioLoaderTest, EmptyTasksArray) {
    const char* json = R"({"tasks": []})";

    auto scenario = load_scenario_from_string(json);
    EXPECT_TRUE(scenario.tasks.empty());
}

TEST_F(ScenarioLoaderTest, NoTasksField) {
    const char* json = "{}";

    auto scenario = load_scenario_from_string(json);
    EXPECT_TRUE(scenario.tasks.empty());
}

TEST_F(ScenarioLoaderTest, TaskWithNoJobs) {
    const char* json = R"({
        "tasks": [{
            "id": 0,
            "period": 10.0,
            "wcet": 2.0,
            "jobs": []
        }]
    })";

    auto scenario = load_scenario_from_string(json);
    ASSERT_EQ(scenario.tasks.size(), 1u);
    EXPECT_TRUE(scenario.tasks[0].jobs.empty());
}

TEST_F(ScenarioLoaderTest, JobsSortedByArrival) {
    const char* json = R"({
        "tasks": [{
            "id": 0,
            "period": 10.0,
            "wcet": 2.0,
            "jobs": [
                {"arrival": 20.0, "duration": 1.0},
                {"arrival": 0.0, "duration": 1.0},
                {"arrival": 10.0, "duration": 1.0}
            ]
        }]
    })";

    auto scenario = load_scenario_from_string(json);
    const auto& jobs = scenario.tasks[0].jobs;

    ASSERT_EQ(jobs.size(), 3u);
    EXPECT_DOUBLE_EQ(time_to_seconds(jobs[0].arrival), 0.0);
    EXPECT_DOUBLE_EQ(time_to_seconds(jobs[1].arrival), 10.0);
    EXPECT_DOUBLE_EQ(time_to_seconds(jobs[2].arrival), 20.0);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(ScenarioLoaderTest, InvalidJsonSyntax) {
    const char* json = "{ invalid }";
    EXPECT_THROW(load_scenario_from_string(json), LoaderError);
}

TEST_F(ScenarioLoaderTest, MissingPeriod) {
    const char* json = R"({
        "tasks": [{"id": 0, "wcet": 2.0}]
    })";
    EXPECT_THROW(load_scenario_from_string(json), LoaderError);
}

TEST_F(ScenarioLoaderTest, MissingWcetAndUtilization) {
    const char* json = R"({
        "tasks": [{"id": 0, "period": 10.0}]
    })";
    EXPECT_THROW(load_scenario_from_string(json), LoaderError);
}

TEST_F(ScenarioLoaderTest, NegativePeriod) {
    const char* json = R"({
        "tasks": [{"id": 0, "period": -10.0, "wcet": 2.0}]
    })";
    EXPECT_THROW(load_scenario_from_string(json), LoaderError);
}

TEST_F(ScenarioLoaderTest, ZeroWcet) {
    const char* json = R"({
        "tasks": [{"id": 0, "period": 10.0, "wcet": 0.0}]
    })";
    EXPECT_THROW(load_scenario_from_string(json), LoaderError);
}

TEST_F(ScenarioLoaderTest, DeadlineLessThanWcet) {
    const char* json = R"({
        "tasks": [{"id": 0, "period": 10.0, "wcet": 5.0, "relative_deadline": 2.0}]
    })";
    EXPECT_THROW(load_scenario_from_string(json), LoaderError);
}

TEST_F(ScenarioLoaderTest, InvalidUtilization) {
    const char* json = R"({
        "tasks": [{"id": 0, "period": 10.0, "utilization": 1.5}]
    })";
    EXPECT_THROW(load_scenario_from_string(json), LoaderError);
}

TEST_F(ScenarioLoaderTest, RootNotObject) {
    const char* json = "[]";
    EXPECT_THROW(load_scenario_from_string(json), LoaderError);
}

// =============================================================================
// Scenario Writer Tests
// =============================================================================

TEST_F(ScenarioLoaderTest, WriteScenarioToStream) {
    ScenarioData scenario;
    TaskParams task;
    task.id = 1;
    task.period = duration_from_seconds(10.0);
    task.relative_deadline = duration_from_seconds(10.0);
    task.wcet = duration_from_seconds(2.0);
    scenario.tasks.push_back(task);

    std::ostringstream out;
    write_scenario_to_stream(scenario, out);

    // Should produce valid JSON that can be re-loaded
    auto loaded = load_scenario_from_string(out.str());
    ASSERT_EQ(loaded.tasks.size(), 1u);
    EXPECT_EQ(loaded.tasks[0].id, 1u);
    EXPECT_DOUBLE_EQ(duration_to_seconds(loaded.tasks[0].period), 10.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(loaded.tasks[0].relative_deadline), 10.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(loaded.tasks[0].wcet), 2.0);
}

TEST_F(ScenarioLoaderTest, WriteScenarioWithJobs) {
    ScenarioData scenario;
    TaskParams task;
    task.id = 0;
    task.period = duration_from_seconds(20.0);
    task.relative_deadline = duration_from_seconds(15.0);
    task.wcet = duration_from_seconds(5.0);
    task.jobs.push_back(JobParams{time_from_seconds(0.0), duration_from_seconds(5.0)});
    task.jobs.push_back(JobParams{time_from_seconds(20.0), duration_from_seconds(4.5)});
    scenario.tasks.push_back(task);

    std::ostringstream out;
    write_scenario_to_stream(scenario, out);

    auto loaded = load_scenario_from_string(out.str());
    ASSERT_EQ(loaded.tasks.size(), 1u);
    ASSERT_EQ(loaded.tasks[0].jobs.size(), 2u);
    EXPECT_DOUBLE_EQ(time_to_seconds(loaded.tasks[0].jobs[0].arrival), 0.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(loaded.tasks[0].jobs[0].duration), 5.0);
    EXPECT_DOUBLE_EQ(time_to_seconds(loaded.tasks[0].jobs[1].arrival), 20.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(loaded.tasks[0].jobs[1].duration), 4.5);
}

TEST_F(ScenarioLoaderTest, WriteScenarioMultipleTasks) {
    ScenarioData scenario;
    for (uint64_t i = 0; i < 3; ++i) {
        TaskParams task;
        task.id = i;
        task.period = duration_from_seconds(10.0 * static_cast<double>(i + 1));
        task.relative_deadline = task.period;
        task.wcet = duration_from_seconds(1.0 * static_cast<double>(i + 1));
        scenario.tasks.push_back(task);
    }

    std::ostringstream out;
    write_scenario_to_stream(scenario, out);

    auto loaded = load_scenario_from_string(out.str());
    ASSERT_EQ(loaded.tasks.size(), 3u);
    for (uint64_t i = 0; i < 3; ++i) {
        EXPECT_EQ(loaded.tasks[i].id, i);
    }
}

TEST_F(ScenarioLoaderTest, WriteEmptyScenario) {
    ScenarioData scenario;

    std::ostringstream out;
    write_scenario_to_stream(scenario, out);

    auto loaded = load_scenario_from_string(out.str());
    EXPECT_TRUE(loaded.tasks.empty());
}

TEST_F(ScenarioLoaderTest, WriteScenarioRoundTrip) {
    // Load a scenario, write it, load it again, verify identical
    const char* json = R"({
        "tasks": [
            {"id": 0, "period": 10.0, "relative_deadline": 8.0, "wcet": 2.0,
             "jobs": [{"arrival": 0.0, "duration": 1.5}, {"arrival": 10.0, "duration": 2.0}]},
            {"id": 1, "period": 25.0, "wcet": 5.0}
        ]
    })";

    auto original = load_scenario_from_string(json);

    std::ostringstream out;
    write_scenario_to_stream(original, out);

    auto reloaded = load_scenario_from_string(out.str());

    ASSERT_EQ(reloaded.tasks.size(), original.tasks.size());
    for (size_t i = 0; i < original.tasks.size(); ++i) {
        EXPECT_EQ(reloaded.tasks[i].id, original.tasks[i].id);
        EXPECT_DOUBLE_EQ(duration_to_seconds(reloaded.tasks[i].period), duration_to_seconds(original.tasks[i].period));
        EXPECT_DOUBLE_EQ(duration_to_seconds(reloaded.tasks[i].relative_deadline),
                         duration_to_seconds(original.tasks[i].relative_deadline));
        EXPECT_DOUBLE_EQ(duration_to_seconds(reloaded.tasks[i].wcet), duration_to_seconds(original.tasks[i].wcet));
        ASSERT_EQ(reloaded.tasks[i].jobs.size(), original.tasks[i].jobs.size());
    }
}
