#include <gtest/gtest.h>
#include <protocols/scenario.hpp>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

using namespace protocols::scenario;

class ScenarioTest : public ::testing::Test {
      protected:
        job test_job;
        task test_task;
        setting test_setting;

        void SetUp() override
        {
                test_job = job{.arrival = 5, .duration = 3};
                test_task = task{.id = 2, .utilization = 10, .period = 3, .jobs = {test_job}};
                test_setting = setting{.tasks = {test_task}};
        }
};

TEST_F(ScenarioTest, JobToJson)
{
        rapidjson::Document result;
        result.SetObject();
        to_json(test_job, result.GetAllocator(), result);

        EXPECT_EQ(result["arrival"].GetDouble(), 5.0);
        EXPECT_EQ(result["duration"].GetDouble(), 3.0);
}

TEST_F(ScenarioTest, TaskToJson)
{
        rapidjson::Document result;
        result.SetObject();
        to_json(test_task, result.GetAllocator(), result);

        EXPECT_EQ(result["id"].GetUint64(), 2);
        EXPECT_EQ(result["period"].GetDouble(), 3.0);
        EXPECT_EQ(result["utilization"].GetDouble(), 10.0);

        ASSERT_EQ(result["jobs"].Size(), 1);
        EXPECT_EQ(result["jobs"][0]["arrival"].GetDouble(), 5.0);
        EXPECT_EQ(result["jobs"][0]["duration"].GetDouble(), 3.0);
}

TEST_F(ScenarioTest, SettingToJson)
{
        rapidjson::Document result;
        result.SetObject();
        to_json(test_setting, result.GetAllocator(), result);

        ASSERT_EQ(result["tasks"].Size(), 1);
        EXPECT_EQ(result["tasks"][0]["id"].GetUint64(), 2);
        EXPECT_EQ(result["tasks"][0]["jobs"][0]["arrival"].GetDouble(), 5.0);
        EXPECT_EQ(result["tasks"][0]["jobs"][0]["duration"].GetDouble(), 3.0);
}

TEST_F(ScenarioTest, FromJsonJob)
{
        rapidjson::Document json_job;
        json_job.SetObject();
        rapidjson::Document::AllocatorType& allocator = json_job.GetAllocator();
        // Create the job JSON data
        json_job.AddMember("arrival", 5.0, allocator);
        json_job.AddMember("duration", 3.0, allocator);

        job parsed_job = from_json_job(json_job);
        EXPECT_DOUBLE_EQ(parsed_job.arrival, 5.0);
        EXPECT_DOUBLE_EQ(parsed_job.duration, 3.0);
}

TEST_F(ScenarioTest, FromJsonTask)
{
        rapidjson::Document json_job;
        json_job.SetObject();
        rapidjson::Document::AllocatorType& allocator = json_job.GetAllocator();
        // Create the job JSON data
        json_job.AddMember("arrival", 5.0, allocator);
        json_job.AddMember("duration", 3.0, allocator);

        rapidjson::Document json_task;
        json_task.SetObject();
        json_task.AddMember("id", 2, json_task.GetAllocator());
        json_task.AddMember("utilization", 10.0, json_task.GetAllocator());
        json_task.AddMember("period", 3.0, json_task.GetAllocator());
        rapidjson::Value jobs_json(rapidjson::kArrayType);
        jobs_json.PushBack(json_job, json_task.GetAllocator());
        json_task.AddMember("jobs", jobs_json, json_task.GetAllocator());

        task parsed_task = from_json_task(json_task);
        EXPECT_EQ(parsed_task.id, 2);
        EXPECT_DOUBLE_EQ(parsed_task.utilization, 10.0);
        EXPECT_DOUBLE_EQ(parsed_task.period, 3.0);

        ASSERT_EQ(parsed_task.jobs.size(), 1);
        EXPECT_DOUBLE_EQ(parsed_task.jobs[0].arrival, 5.0);
        EXPECT_DOUBLE_EQ(parsed_task.jobs[0].duration, 3.0);
}

TEST_F(ScenarioTest, FromJsonSetting)
{
        rapidjson::Document json_job;
        json_job.SetObject();
        rapidjson::Document::AllocatorType& allocator = json_job.GetAllocator();
        // Create the job JSON data
        json_job.AddMember("arrival", 5.0, allocator);
        json_job.AddMember("duration", 3.0, allocator);

        rapidjson::Document json_task;
        json_task.SetObject();
        json_task.AddMember("id", 2, json_task.GetAllocator());
        json_task.AddMember("utilization", 10.0, json_task.GetAllocator());
        json_task.AddMember("period", 3.0, json_task.GetAllocator());
        rapidjson::Value jobs_json(rapidjson::kArrayType);
        jobs_json.PushBack(json_job, json_task.GetAllocator());
        json_task.AddMember("jobs", jobs_json, json_task.GetAllocator());

        rapidjson::Document json_setting;
        json_setting.SetObject();
        rapidjson::Value tasks_json(rapidjson::kArrayType);
        tasks_json.PushBack(json_task, json_setting.GetAllocator());
        json_setting.AddMember("tasks", tasks_json, json_setting.GetAllocator());

        setting parsed_setting = from_json_setting(json_setting);
        ASSERT_EQ(parsed_setting.tasks.size(), 1);
        EXPECT_EQ(parsed_setting.tasks[0].id, 2);
        EXPECT_EQ(parsed_setting.tasks[0].jobs.size(), 1);
        EXPECT_DOUBLE_EQ(parsed_setting.tasks[0].jobs[0].arrival, 5.0);
}

class ScenarioFileIOTest : public ::testing::Test {
      protected:
        std::filesystem::path temp_file;
        setting test_setting;

        void SetUp() override
        {
                temp_file = std::filesystem::temp_directory_path() / "scenario_test.json";
                test_setting = setting{
                    .tasks = {
                        {.id = 2,
                         .utilization = 10,
                         .period = 3,
                         .jobs = {{.arrival = 5, .duration = 3}}}}};
        }

        void TearDown() override
        {
                if (std::filesystem::exists(temp_file)) { std::filesystem::remove(temp_file); }
        }
};

TEST_F(ScenarioFileIOTest, WriteAndReadFile)
{
        setting read_setting{};
        EXPECT_NO_THROW(write_file(temp_file, test_setting));
        EXPECT_NO_THROW(read_setting = read_file(temp_file));

        ASSERT_EQ(read_setting.tasks.size(), test_setting.tasks.size());
        ASSERT_EQ(read_setting.tasks[0].id, test_setting.tasks[0].id);
}

TEST_F(ScenarioFileIOTest, WriteFileErrorHandling)
{
        std::filesystem::path invalid_path = "/invalid/path/scenario_test.json";
        EXPECT_THROW(write_file(invalid_path, test_setting), std::runtime_error);
}

TEST_F(ScenarioFileIOTest, ReadFileErrorHandling)
{
        std::filesystem::path non_existent_file = "non_existent_file.json";
        EXPECT_THROW(read_file(non_existent_file), std::runtime_error);
}
