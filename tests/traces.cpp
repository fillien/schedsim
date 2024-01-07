#include "traces.hpp"
#include "nlohmann/json.hpp"
#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <variant>

class TraceEventTest : public ::testing::Test {};

TEST_F(TraceEventTest, ReschedJsonTest)
{
        traces::resched original;
        auto json = traces::to_json(original);
        auto converted = traces::from_json(json);
}

TEST_F(TraceEventTest, SimFinishedJsonTest)
{
        traces::sim_finished original;
        auto json = traces::to_json(original);
        auto converted = traces::from_json(json);
}

TEST_F(TraceEventTest, JobArrivalJsonTest)
{
        traces::job_arrival original{1, 10.0, 20.0};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::job_arrival>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_DOUBLE_EQ(converted.duration, original.duration);
        EXPECT_DOUBLE_EQ(converted.deadline, original.deadline);
}

TEST_F(TraceEventTest, JobFinishedJsonTest)
{
        traces::job_finished original{2};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::job_finished>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, ProcActivatedJsonTest)
{
        traces::proc_activated original{3};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::proc_activated>(traces::from_json(json));
        EXPECT_EQ(converted.proc_id, original.proc_id);
}

TEST_F(TraceEventTest, ProcIdledJsonTest)
{
        traces::proc_idled original{4};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::proc_idled>(traces::from_json(json));
        EXPECT_EQ(converted.proc_id, original.proc_id);
}

TEST_F(TraceEventTest, ServBudgetReplenishedJsonTest)
{
        traces::serv_budget_replenished original{5, 50.0};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::serv_budget_replenished>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_DOUBLE_EQ(converted.budget, original.budget);
}

TEST_F(TraceEventTest, ServInactiveJsonTest)
{
        traces::serv_inactive original{6};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::serv_inactive>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, ServBudgetExhaustedJsonTest)
{
        traces::serv_budget_exhausted original{7};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::serv_budget_exhausted>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, ServNonContJsonTest)
{
        traces::serv_non_cont original{8};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::serv_non_cont>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, ServPostponeJsonTest)
{
        traces::serv_postpone original{9, 60.0};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::serv_postpone>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_DOUBLE_EQ(converted.deadline, original.deadline);
}

TEST_F(TraceEventTest, ServReadyJsonTest)
{
        traces::serv_ready original{10, 70.0};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::serv_ready>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_DOUBLE_EQ(converted.deadline, original.deadline);
}

TEST_F(TraceEventTest, ServRunningJsonTest)
{
        traces::serv_running original{11};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::serv_running>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, TaskPreemptedJsonTest)
{
        traces::task_preempted original{12};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::task_preempted>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, TaskScheduledJsonTest)
{
        traces::task_scheduled original{13, 14};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::task_scheduled>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_EQ(converted.proc_id, original.proc_id);
}

TEST_F(TraceEventTest, TaskRejectedJsonTest)
{
        traces::task_rejected original{15};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::task_rejected>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
}

TEST_F(TraceEventTest, VirtualTimeUpdateJsonTest)
{
        traces::virtual_time_update original{16, 80.0};
        auto json = traces::to_json(original);
        auto converted = std::get<traces::virtual_time_update>(traces::from_json(json));
        EXPECT_EQ(converted.task_id, original.task_id);
        EXPECT_DOUBLE_EQ(converted.virtual_time, original.virtual_time);
}

TEST_F(TraceEventTest, LogFileWriteRead)
{
        std::multimap<double, traces::trace> logs;
        std::multimap<double, traces::trace> read_logs;

        logs.insert({1.0, traces::job_arrival{.task_id = 1, .duration = 2.5, .deadline = 3.5}});

        std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "trace_test.log";
        EXPECT_NO_THROW(traces::write_log_file(logs, temp_file));
        EXPECT_NO_THROW(read_logs = traces::read_log_file(temp_file));

        ASSERT_EQ(read_logs.size(), logs.size());

        // Clean up: remove the temporary file
        if (std::filesystem::exists(temp_file)) { std::filesystem::remove(temp_file); }
}
