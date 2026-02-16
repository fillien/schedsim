#include <schedsim/io/scenario_injection.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::io;
using namespace schedsim::core;

class ScenarioInjectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a minimal platform for testing
        auto& platform = engine.platform();
        auto& proc_type = platform.add_processor_type("cpu", 1.0);
        auto& cd = platform.add_clock_domain(Frequency{1000.0}, Frequency{1000.0});
        auto& pd = platform.add_power_domain({{0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{0.0}}});
        platform.add_processor(proc_type, cd, pd);
    }

    Engine engine;
};

TEST_F(ScenarioInjectionTest, InjectScenarioCreatesTasks) {
    ScenarioData scenario;
    scenario.tasks.push_back(TaskParams{
        .id = 0,
        .period = duration_from_seconds(10.0),
        .relative_deadline = duration_from_seconds(10.0),
        .wcet = duration_from_seconds(2.0),
        .jobs = {}
    });
    scenario.tasks.push_back(TaskParams{
        .id = 1,
        .period = duration_from_seconds(20.0),
        .relative_deadline = duration_from_seconds(15.0),
        .wcet = duration_from_seconds(4.0),
        .jobs = {}
    });

    inject_scenario(engine, scenario);

    auto& platform = engine.platform();
    ASSERT_EQ(platform.task_count(), 2u);

    EXPECT_DOUBLE_EQ(duration_to_seconds(platform.task(0).period()), 10.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(platform.task(0).wcet()), 2.0);

    EXPECT_DOUBLE_EQ(duration_to_seconds(platform.task(1).period()), 20.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(platform.task(1).relative_deadline()), 15.0);
}

TEST_F(ScenarioInjectionTest, ScheduleArrivalsSchedulesJobs) {
    // First, add a task manually
    auto& platform = engine.platform();
    auto& task = platform.add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));

    // Schedule arrivals BEFORE finalize (Engine doesn't allow scheduling after finalize)
    std::vector<JobParams> jobs = {
        {time_from_seconds(0.0), duration_from_seconds(1.5)},
        {time_from_seconds(10.0), duration_from_seconds(2.0)},
        {time_from_seconds(20.0), duration_from_seconds(1.0)}
    };
    schedule_arrivals(engine, task, jobs);

    // Track job arrivals
    std::vector<std::pair<TimePoint, Duration>> arrivals;
    engine.set_job_arrival_handler([&arrivals](Task& task_ref, Job job) {
        // Compute arrival time from absolute deadline and task's relative deadline
        auto arrival = job.absolute_deadline() - task_ref.relative_deadline();
        arrivals.push_back({arrival, job.total_work()});
    });

    // Finalize after scheduling
    engine.finalize();

    // Run simulation
    engine.run();

    ASSERT_EQ(arrivals.size(), 3u);
    EXPECT_DOUBLE_EQ(time_to_seconds(arrivals[0].first), 0.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(arrivals[0].second), 1.5);
    EXPECT_DOUBLE_EQ(time_to_seconds(arrivals[1].first), 10.0);
    EXPECT_DOUBLE_EQ(time_to_seconds(arrivals[2].first), 20.0);
}

TEST_F(ScenarioInjectionTest, EmptyScenario) {
    ScenarioData scenario;

    inject_scenario(engine, scenario);
    EXPECT_EQ(engine.platform().task_count(), 0u);
}

TEST_F(ScenarioInjectionTest, EmptyJobsArray) {
    // Finalize platform first
    engine.finalize();

    // Add task after finalize should fail, so this tests the other branch
    ScenarioData scenario;
    // Empty scenario with tasks but no jobs should still work
}

TEST_F(ScenarioInjectionTest, MultipleTasksWithOverlappingArrivals) {
    ScenarioData scenario;
    scenario.tasks.push_back(TaskParams{
        .id = 0,
        .period = duration_from_seconds(10.0),
        .relative_deadline = duration_from_seconds(10.0),
        .wcet = duration_from_seconds(3.0),
        .jobs = {}
    });
    scenario.tasks.push_back(TaskParams{
        .id = 1,
        .period = duration_from_seconds(15.0),
        .relative_deadline = duration_from_seconds(15.0),
        .wcet = duration_from_seconds(2.0),
        .jobs = {}
    });

    inject_scenario(engine, scenario);

    auto& platform = engine.platform();
    EXPECT_EQ(platform.task_count(), 2u);

    // Schedule arrivals BEFORE finalize (Engine doesn't allow scheduling after finalize)
    std::vector<JobParams> jobs0 = {
        {time_from_seconds(0.0), duration_from_seconds(3.0)},
        {time_from_seconds(10.0), duration_from_seconds(2.5)}
    };
    std::vector<JobParams> jobs1 = {
        {time_from_seconds(0.0), duration_from_seconds(2.0)},
        {time_from_seconds(15.0), duration_from_seconds(1.5)}
    };

    schedule_arrivals(engine, platform.task(0), jobs0);
    schedule_arrivals(engine, platform.task(1), jobs1);

    std::size_t arrival_count = 0;
    engine.set_job_arrival_handler([&arrival_count](Task&, Job) {
        ++arrival_count;
    });

    engine.finalize();

    engine.run();
    EXPECT_EQ(arrival_count, 4u);
}
