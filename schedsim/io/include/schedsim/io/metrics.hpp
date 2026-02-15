#pragma once

#include <schedsim/io/trace_writers.hpp>

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace schedsim::io {

struct SimulationMetrics {
    // Scheduling metrics
    uint64_t total_jobs{0};
    uint64_t completed_jobs{0};
    uint64_t deadline_misses{0};
    uint64_t preemptions{0};
    uint64_t context_switches{0};

    // Energy metrics
    double total_energy_mj{0.0};
    std::unordered_map<uint64_t, double> energy_per_processor;

    // Utilization
    double average_utilization{0.0};
    std::unordered_map<uint64_t, double> utilization_per_processor;

    // Response times per task (task_id -> list of response times)
    std::unordered_map<uint64_t, std::vector<double>> response_times_per_task;

    // Per-task deadline miss counts
    std::unordered_map<uint64_t, uint64_t> deadline_misses_per_task;

    // Rejected task count
    uint64_t rejected_tasks{0};

    // Waiting times per task (arrival -> job_start delay)
    std::unordered_map<uint64_t, std::vector<double>> waiting_times_per_task;

    // Frequency change log
    struct FrequencyChange {
        double time;
        uint64_t clock_domain_id;
        double old_freq_mhz;
        double new_freq_mhz;
    };
    std::vector<FrequencyChange> frequency_changes;
};

// Compute metrics from in-memory traces
SimulationMetrics compute_metrics(const std::vector<TraceRecord>& traces);

// Compute metrics from JSON trace file
SimulationMetrics compute_metrics_from_file(const std::filesystem::path& path);

// Summary statistics for response times
struct ResponseTimeStats {
    double min{0.0};
    double max{0.0};
    double mean{0.0};
    double median{0.0};
    double stddev{0.0};
    double percentile_95{0.0};
    double percentile_99{0.0};
};

ResponseTimeStats compute_response_time_stats(const std::vector<double>& response_times);

} // namespace schedsim::io
