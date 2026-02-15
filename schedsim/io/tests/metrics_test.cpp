#include <schedsim/io/metrics.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace schedsim::io;

class MetricsTest : public ::testing::Test {
protected:
    std::vector<TraceRecord> create_trace_records() {
        std::vector<TraceRecord> traces;

        // Job arrivals
        traces.push_back(TraceRecord{
            .time = 0.0,
            .type = "job_arrival",
            .fields = {{"task_id", uint64_t{0}}, {"job_id", uint64_t{0}}}
        });
        traces.push_back(TraceRecord{
            .time = 0.0,
            .type = "job_arrival",
            .fields = {{"task_id", uint64_t{1}}, {"job_id", uint64_t{0}}}
        });
        traces.push_back(TraceRecord{
            .time = 10.0,
            .type = "job_arrival",
            .fields = {{"task_id", uint64_t{0}}, {"job_id", uint64_t{1}}}
        });

        // Job completions
        traces.push_back(TraceRecord{
            .time = 2.0,
            .type = "job_completion",
            .fields = {{"task_id", uint64_t{0}}, {"job_id", uint64_t{0}}}
        });
        traces.push_back(TraceRecord{
            .time = 5.0,
            .type = "job_completion",
            .fields = {{"task_id", uint64_t{1}}, {"job_id", uint64_t{0}}}
        });
        traces.push_back(TraceRecord{
            .time = 12.0,
            .type = "job_completion",
            .fields = {{"task_id", uint64_t{0}}, {"job_id", uint64_t{1}}}
        });

        return traces;
    }
};

// =============================================================================
// Basic Metrics Tests
// =============================================================================

TEST_F(MetricsTest, EmptyTrace) {
    std::vector<TraceRecord> traces;
    auto metrics = compute_metrics(traces);

    EXPECT_EQ(metrics.total_jobs, 0u);
    EXPECT_EQ(metrics.completed_jobs, 0u);
    EXPECT_EQ(metrics.deadline_misses, 0u);
}

TEST_F(MetricsTest, JobCountsBasic) {
    auto traces = create_trace_records();
    auto metrics = compute_metrics(traces);

    EXPECT_EQ(metrics.total_jobs, 3u);
    EXPECT_EQ(metrics.completed_jobs, 3u);
}

TEST_F(MetricsTest, ResponseTimes) {
    auto traces = create_trace_records();
    auto metrics = compute_metrics(traces);

    // Task 0: arrivals at 0, 10; completions at 2, 12
    // Response times: 2, 2
    ASSERT_EQ(metrics.response_times_per_task.count(0), 1u);
    const auto& rt0 = metrics.response_times_per_task.at(0);
    ASSERT_EQ(rt0.size(), 2u);
    EXPECT_DOUBLE_EQ(rt0[0], 2.0);
    EXPECT_DOUBLE_EQ(rt0[1], 2.0);

    // Task 1: arrival at 0, completion at 5
    // Response time: 5
    ASSERT_EQ(metrics.response_times_per_task.count(1), 1u);
    const auto& rt1 = metrics.response_times_per_task.at(1);
    ASSERT_EQ(rt1.size(), 1u);
    EXPECT_DOUBLE_EQ(rt1[0], 5.0);
}

TEST_F(MetricsTest, DeadlineMisses) {
    std::vector<TraceRecord> traces;
    traces.push_back(TraceRecord{.time = 10.0, .type = "deadline_miss", .fields = {}});
    traces.push_back(TraceRecord{.time = 20.0, .type = "deadline_miss", .fields = {}});
    traces.push_back(TraceRecord{.time = 30.0, .type = "deadline_miss", .fields = {}});

    auto metrics = compute_metrics(traces);
    EXPECT_EQ(metrics.deadline_misses, 3u);
}

TEST_F(MetricsTest, Preemptions) {
    std::vector<TraceRecord> traces;
    traces.push_back(TraceRecord{.time = 1.0, .type = "preemption", .fields = {}});
    traces.push_back(TraceRecord{.time = 2.0, .type = "preemption", .fields = {}});

    auto metrics = compute_metrics(traces);
    EXPECT_EQ(metrics.preemptions, 2u);
}

TEST_F(MetricsTest, ContextSwitches) {
    std::vector<TraceRecord> traces;
    traces.push_back(TraceRecord{.time = 1.0, .type = "context_switch", .fields = {}});
    traces.push_back(TraceRecord{.time = 2.0, .type = "context_switch", .fields = {}});
    traces.push_back(TraceRecord{.time = 3.0, .type = "context_switch", .fields = {}});

    auto metrics = compute_metrics(traces);
    EXPECT_EQ(metrics.context_switches, 3u);
}

TEST_F(MetricsTest, EnergyMetrics) {
    std::vector<TraceRecord> traces;
    traces.push_back(TraceRecord{
        .time = 10.0,
        .type = "energy",
        .fields = {{"proc", uint64_t{0}}, {"energy_mj", 100.0}}
    });
    traces.push_back(TraceRecord{
        .time = 20.0,
        .type = "energy",
        .fields = {{"proc", uint64_t{0}}, {"energy_mj", 150.0}}
    });
    traces.push_back(TraceRecord{
        .time = 20.0,
        .type = "energy",
        .fields = {{"proc", uint64_t{1}}, {"energy_mj", 80.0}}
    });

    auto metrics = compute_metrics(traces);

    EXPECT_DOUBLE_EQ(metrics.total_energy_mj, 330.0);
    EXPECT_DOUBLE_EQ(metrics.energy_per_processor[0], 250.0);
    EXPECT_DOUBLE_EQ(metrics.energy_per_processor[1], 80.0);
}

// =============================================================================
// Response Time Stats Tests
// =============================================================================

TEST_F(MetricsTest, ResponseTimeStatsEmpty) {
    std::vector<double> response_times;
    auto stats = compute_response_time_stats(response_times);

    EXPECT_DOUBLE_EQ(stats.min, 0.0);
    EXPECT_DOUBLE_EQ(stats.max, 0.0);
    EXPECT_DOUBLE_EQ(stats.mean, 0.0);
}

TEST_F(MetricsTest, ResponseTimeStatsSingle) {
    std::vector<double> response_times = {5.0};
    auto stats = compute_response_time_stats(response_times);

    EXPECT_DOUBLE_EQ(stats.min, 5.0);
    EXPECT_DOUBLE_EQ(stats.max, 5.0);
    EXPECT_DOUBLE_EQ(stats.mean, 5.0);
    EXPECT_DOUBLE_EQ(stats.median, 5.0);
    EXPECT_DOUBLE_EQ(stats.stddev, 0.0);
}

TEST_F(MetricsTest, ResponseTimeStatsBasic) {
    std::vector<double> response_times = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto stats = compute_response_time_stats(response_times);

    EXPECT_DOUBLE_EQ(stats.min, 1.0);
    EXPECT_DOUBLE_EQ(stats.max, 5.0);
    EXPECT_DOUBLE_EQ(stats.mean, 3.0);
    EXPECT_DOUBLE_EQ(stats.median, 3.0);  // Odd number of elements

    // Stddev: sqrt(sum((x-mean)^2)/n) = sqrt(2) ≈ 1.414
    EXPECT_NEAR(stats.stddev, std::sqrt(2.0), 1e-10);
}

TEST_F(MetricsTest, ResponseTimeStatsEvenMedian) {
    std::vector<double> response_times = {1.0, 2.0, 3.0, 4.0};
    auto stats = compute_response_time_stats(response_times);

    // Median of even elements: (2 + 3) / 2 = 2.5
    EXPECT_DOUBLE_EQ(stats.median, 2.5);
}

TEST_F(MetricsTest, ResponseTimeStatsPercentiles) {
    // Create 100 values: 1, 2, 3, ..., 100
    std::vector<double> response_times;
    for (int idx = 1; idx <= 100; ++idx) {
        response_times.push_back(static_cast<double>(idx));
    }

    auto stats = compute_response_time_stats(response_times);

    // 95th percentile should be around 95
    EXPECT_NEAR(stats.percentile_95, 95.0, 1.0);

    // 99th percentile should be around 99
    EXPECT_NEAR(stats.percentile_99, 99.0, 1.0);
}

TEST_F(MetricsTest, ResponseTimeStatsUnsorted) {
    // Stats should work regardless of input order
    std::vector<double> response_times = {5.0, 1.0, 3.0, 2.0, 4.0};
    auto stats = compute_response_time_stats(response_times);

    EXPECT_DOUBLE_EQ(stats.min, 1.0);
    EXPECT_DOUBLE_EQ(stats.max, 5.0);
    EXPECT_DOUBLE_EQ(stats.mean, 3.0);
    EXPECT_DOUBLE_EQ(stats.median, 3.0);
}

// =============================================================================
// Extended Metrics Tests (GAPS 28-31)
// =============================================================================

TEST_F(MetricsTest, DeadlineMissesPerTask) {
    std::vector<TraceRecord> traces;
    // Task 0 misses 2 deadlines
    traces.push_back(TraceRecord{
        .time = 10.0, .type = "deadline_miss",
        .fields = {{"task_id", uint64_t{0}}}
    });
    traces.push_back(TraceRecord{
        .time = 20.0, .type = "deadline_miss",
        .fields = {{"task_id", uint64_t{0}}}
    });
    // Task 1 misses 1 deadline
    traces.push_back(TraceRecord{
        .time = 15.0, .type = "deadline_miss",
        .fields = {{"task_id", uint64_t{1}}}
    });

    auto metrics = compute_metrics(traces);

    EXPECT_EQ(metrics.deadline_misses, 3u);
    EXPECT_EQ(metrics.deadline_misses_per_task.at(0), 2u);
    EXPECT_EQ(metrics.deadline_misses_per_task.at(1), 1u);
}

TEST_F(MetricsTest, RejectedTasks) {
    std::vector<TraceRecord> traces;
    traces.push_back(TraceRecord{
        .time = 1.0, .type = "task_rejected",
        .fields = {{"task_id", uint64_t{5}}}
    });
    traces.push_back(TraceRecord{
        .time = 2.0, .type = "task_rejected",
        .fields = {{"task_id", uint64_t{6}}}
    });

    auto metrics = compute_metrics(traces);
    EXPECT_EQ(metrics.rejected_tasks, 2u);
}

TEST_F(MetricsTest, FrequencyChanges) {
    std::vector<TraceRecord> traces;
    traces.push_back(TraceRecord{
        .time = 5.0, .type = "frequency_change",
        .fields = {
            {"clock_domain_id", uint64_t{0}},
            {"old_freq_mhz", 2000.0},
            {"new_freq_mhz", 1000.0}
        }
    });
    traces.push_back(TraceRecord{
        .time = 10.0, .type = "frequency_change",
        .fields = {
            {"clock_domain_id", uint64_t{1}},
            {"old_freq_mhz", 1500.0},
            {"new_freq_mhz", 500.0}
        }
    });

    auto metrics = compute_metrics(traces);

    ASSERT_EQ(metrics.frequency_changes.size(), 2u);
    EXPECT_DOUBLE_EQ(metrics.frequency_changes[0].time, 5.0);
    EXPECT_EQ(metrics.frequency_changes[0].clock_domain_id, 0u);
    EXPECT_DOUBLE_EQ(metrics.frequency_changes[0].old_freq_mhz, 2000.0);
    EXPECT_DOUBLE_EQ(metrics.frequency_changes[0].new_freq_mhz, 1000.0);
    EXPECT_DOUBLE_EQ(metrics.frequency_changes[1].time, 10.0);
    EXPECT_EQ(metrics.frequency_changes[1].clock_domain_id, 1u);
}

TEST_F(MetricsTest, WaitingTimes) {
    std::vector<TraceRecord> traces;
    // Task 0, Job 0: arrives at t=0, starts at t=1
    traces.push_back(TraceRecord{
        .time = 0.0, .type = "job_arrival",
        .fields = {{"task_id", uint64_t{0}}, {"job_id", uint64_t{0}}}
    });
    traces.push_back(TraceRecord{
        .time = 1.0, .type = "job_start",
        .fields = {{"task_id", uint64_t{0}}, {"job_id", uint64_t{0}}}
    });
    // Task 0, Job 1: arrives at t=5, starts at t=8
    traces.push_back(TraceRecord{
        .time = 5.0, .type = "job_arrival",
        .fields = {{"task_id", uint64_t{0}}, {"job_id", uint64_t{1}}}
    });
    traces.push_back(TraceRecord{
        .time = 8.0, .type = "job_start",
        .fields = {{"task_id", uint64_t{0}}, {"job_id", uint64_t{1}}}
    });

    auto metrics = compute_metrics(traces);

    ASSERT_EQ(metrics.waiting_times_per_task.count(0), 1u);
    const auto& wt = metrics.waiting_times_per_task.at(0);
    ASSERT_EQ(wt.size(), 2u);
    EXPECT_DOUBLE_EQ(wt[0], 1.0);  // 1.0 - 0.0
    EXPECT_DOUBLE_EQ(wt[1], 3.0);  // 8.0 - 5.0
}

TEST_F(MetricsTest, WaitingTimesMissingStart) {
    std::vector<TraceRecord> traces;
    // Arrival without a matching job_start — should not crash or produce entry
    traces.push_back(TraceRecord{
        .time = 0.0, .type = "job_arrival",
        .fields = {{"task_id", uint64_t{0}}, {"job_id", uint64_t{0}}}
    });

    auto metrics = compute_metrics(traces);

    // No waiting time should be recorded since there's no job_start
    EXPECT_TRUE(metrics.waiting_times_per_task.empty());
}
