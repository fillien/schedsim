#include "nlohmann/json.hpp"
#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <protocols/traces.hpp>
#include <variant>

using namespace protocols::traces;

class TraceEventTest : public ::testing::Test {};

/*
TEST_F(TraceEventTest, ReschedJsonTest)
{
        resched original;
        auto json = to_json(original);
        auto converted = from_json(json);
}

TEST_F(TraceEventTest, SimFinishedJsonTest)
{
        sim_finished original;
        auto json = to_json(original);
        auto converted = from_json(json);
}

TEST_F(TraceEventTest, JobArrivalJsonTest)
{
        job_arrival original{1, 10.0, 20.0};
        auto json = to_json(original);
        auto converted = std::get<job_arrival>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_DOUBLE_EQ(converted.duration, original.duration);
        EXPECT_DOUBLE_EQ(converted.deadline, original.deadline);
}

TEST_F(TraceEventTest, JobFinishedJsonTest)
{
        job_finished original{2};
        auto json = to_json(original);
        auto converted = std::get<job_finished>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, ProcActivatedJsonTest)
{
        proc_activated original{3};
        auto json = to_json(original);
        auto converted = std::get<proc_activated>(from_json(json));
        EXPECT_EQ(converted.proc_id, original.proc_id);
}

TEST_F(TraceEventTest, ProcIdledJsonTest)
{
        proc_idled original{4};
        auto json = to_json(original);
        auto converted = std::get<proc_idled>(from_json(json));
        EXPECT_EQ(converted.proc_id, original.proc_id);
}

TEST_F(TraceEventTest, ProcSleepJsonTest)
{
        proc_sleep original{5};
        auto json = to_json(original);
        auto converted = std::get<proc_sleep>(from_json(json));
        EXPECT_EQ(converted.proc_id, original.proc_id);
}

TEST_F(TraceEventTest, ServBudgetReplenishedJsonTest)
{
        serv_budget_replenished original{5, 50.0};
        auto json = to_json(original);
        auto converted = std::get<serv_budget_replenished>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_DOUBLE_EQ(converted.budget, original.budget);
}

TEST_F(TraceEventTest, ServInactiveJsonTest)
{
        serv_inactive original{6, 0.2};
        auto json = to_json(original);
        auto converted = std::get<serv_inactive>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, ServBudgetExhaustedJsonTest)
{
        serv_budget_exhausted original{7};
        auto json = to_json(original);
        auto converted = std::get<serv_budget_exhausted>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, ServNonContJsonTest)
{
        serv_non_cont original{8};
        auto json = to_json(original);
        auto converted = std::get<serv_non_cont>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, ServPostponeJsonTest)
{
        serv_postpone original{9, 60.0};
        auto json = to_json(original);
        auto converted = std::get<serv_postpone>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_DOUBLE_EQ(converted.deadline, original.deadline);
}

TEST_F(TraceEventTest, ServReadyJsonTest)
{
        serv_ready original{10, 70.0, 0.2};
        auto json = to_json(original);
        auto converted = std::get<serv_ready>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_DOUBLE_EQ(converted.deadline, original.deadline);
}

TEST_F(TraceEventTest, ServRunningJsonTest)
{
        serv_running original{11};
        auto json = to_json(original);
        auto converted = std::get<serv_running>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, TaskPreemptedJsonTest)
{
        task_preempted original{12};
        auto json = to_json(original);
        auto converted = std::get<task_preempted>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, TaskScheduledJsonTest)
{
        task_scheduled original{13, 14};
        auto json = to_json(original);
        auto converted = std::get<task_scheduled>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_EQ(converted.proc_id, original.proc_id);
}

TEST_F(TraceEventTest, TaskRejectedJsonTest)
{
        task_rejected original{15};
        auto json = to_json(original);
        auto converted = std::get<task_rejected>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, VirtualTimeUpdateJsonTest)
{
        virtual_time_update original{16, 80.0};
        auto json = to_json(original);
        auto converted = std::get<virtual_time_update>(from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_DOUBLE_EQ(converted.virtual_time, original.virtual_time);
}

TEST_F(TraceEventTest, FrequencyUpdateJsonTest)
{
        frequency_update original{120};
        auto json = to_json(original);
        auto converted = std::get<frequency_update>(from_json(json));
        EXPECT_DOUBLE_EQ(converted.frequency, original.frequency);
}

TEST_F(TraceEventTest, LogFileWriteRead)
{
        std::multimap<double, trace> logs;
        std::multimap<double, trace> read_logs;

        logs.insert({1.0, job_arrival{.task_id = 1, .duration = 2.5, .deadline = 3.5}});

        std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "trace_test.log";
        EXPECT_NO_THROW(write_log_file(logs, temp_file));
        EXPECT_NO_THROW(read_logs = read_log_file(temp_file));

        ASSERT_EQ(read_logs.size(), logs.size());

        // Clean up: remove the temporary file
        if (std::filesystem::exists(temp_file)) { std::filesystem::remove(temp_file); }
}
*/
