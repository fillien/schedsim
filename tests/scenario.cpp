#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <scenario.hpp>

#include <iostream>

class ScenarioTest : public ::testing::Test {
      protected:
        nlohmann::json json_job, json_task, json_setting;
        scenario::job test_job;
        scenario::task test_task;
        scenario::setting test_setting;

        void SetUp() override
        {
                using namespace scenario;
                test_job = job{.arrival = 5, .duration = 3};
                test_task = task{.id = 2, .utilization = 10, .period = 3, .jobs = {test_job}};
                test_setting = setting{.tasks = {test_task}};
                json_job = {{"arrival", 5.0}, {"duration", 3.0}};
                json_task = {
                    {"id", 2}, {"utilization", 10.0}, {"period", 3.0}, {"jobs", {json_job}}};
                json_setting = {{"tasks", {json_task}}};
        }
};

TEST_F(ScenarioTest, JobToJson)
{
        nlohmann::json result = scenario::to_json(test_job);
        EXPECT_EQ(result["arrival"], 5);
        EXPECT_EQ(result["duration"], 3);
}

TEST_F(ScenarioTest, TaskToJson)
{
        nlohmann::json result = scenario::to_json(test_task);
        EXPECT_EQ(result["id"], 2);
        EXPECT_EQ(result["period"], 3);
        EXPECT_EQ(result["utilization"], 10);
        ASSERT_EQ(result["jobs"].size(), 1);
        EXPECT_EQ(result["jobs"][0]["arrival"], 5);
        EXPECT_EQ(result["jobs"][0]["duration"], 3);
}

TEST_F(ScenarioTest, SettingToJson)
{
        nlohmann::json result = scenario::to_json(test_setting);
        ASSERT_EQ(result["tasks"].size(), 1);
        EXPECT_EQ(result["tasks"][0]["id"], 2);
        EXPECT_EQ(result["tasks"][0]["jobs"][0]["arrival"], 5);
        EXPECT_EQ(result["tasks"][0]["jobs"][0]["duration"], 3);
}

TEST_F(ScenarioTest, FromJsonJob)
{
        auto job = scenario::from_json_job(json_job);
        EXPECT_DOUBLE_EQ(job.arrival, 5.0);
        EXPECT_DOUBLE_EQ(job.duration, 3.0);
}

TEST_F(ScenarioTest, FromJsonTask)
{
        auto task = scenario::from_json_task(json_task);
        EXPECT_EQ(task.id, 2);
        EXPECT_DOUBLE_EQ(task.utilization, 10.0);
        EXPECT_DOUBLE_EQ(task.period, 3.0);
        ASSERT_EQ(task.jobs.size(), 1);
        EXPECT_DOUBLE_EQ(task.jobs[0].arrival, 5.0);
        EXPECT_DOUBLE_EQ(task.jobs[0].duration, 3.0);
}

TEST_F(ScenarioTest, FromJsonSetting)
{
        auto setting = scenario::from_json_setting(json_setting);
        ASSERT_EQ(setting.tasks.size(), 1);
        EXPECT_EQ(setting.tasks[0].id, 2);
        EXPECT_EQ(setting.tasks[0].jobs.size(), 1);
        EXPECT_DOUBLE_EQ(setting.tasks[0].jobs[0].arrival, 5.0);
}

class ScenarioFileIOTest : public ::testing::Test {
      protected:
        std::filesystem::path temp_file;
        scenario::setting test_setting;

        void SetUp() override
        {
                temp_file = std::filesystem::temp_directory_path() / "scenario_test.json";
                test_setting = {
                    .tasks = {
                        {.id = 2,
                         .utilization = 10,
                         .period = 3,
                         .jobs = {{.arrival = 5, .duration = 3}}}}};
        }

        void TearDown() override
        {
                // Clean up: remove the temporary file if it exists
                if (std::filesystem::exists(temp_file)) { std::filesystem::remove(temp_file); }
        }
};

TEST_F(ScenarioFileIOTest, WriteAndReadFile)
{
        scenario::setting read_setting{};
        EXPECT_NO_THROW(scenario::write_file(temp_file, test_setting));
        EXPECT_NO_THROW(read_setting = scenario::read_file(temp_file));

        ASSERT_EQ(read_setting.tasks.size(), test_setting.tasks.size());
        ASSERT_EQ(read_setting.tasks[0].id, test_setting.tasks[0].id);
}

TEST_F(ScenarioFileIOTest, WriteFileErrorHandling)
{
        std::filesystem::path invalid_path = "/invalid/path/scenario_test.json";
        EXPECT_THROW(scenario::write_file(invalid_path, test_setting), std::runtime_error);
}

TEST_F(ScenarioFileIOTest, ReadFileErrorHandling)
{
        std::filesystem::path non_existent_file = "non_existent_file.json";
        EXPECT_THROW(scenario::read_file(non_existent_file), std::runtime_error);
}
