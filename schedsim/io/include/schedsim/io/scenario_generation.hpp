#pragma once

#include <schedsim/io/scenario_loader.hpp>

#include <cstddef>
#include <random>
#include <vector>

namespace schedsim::io {

// UUniFast algorithm: generate n utilizations summing to U
std::vector<double> uunifast(std::size_t num_tasks, double target_utilization, std::mt19937& rng);

// Period distribution options
struct PeriodDistribution {
    core::Duration min{};
    core::Duration max{};
    bool log_uniform{true};  // log-uniform distribution (common in RT systems)
};

// Generate task parameters with target total utilization
std::vector<TaskParams> generate_task_set(
    std::size_t num_tasks,
    double target_utilization,
    PeriodDistribution period_dist,
    std::mt19937& rng);

// Generate job arrivals for a task set over a duration
// exec_ratio: actual execution time / wcet (1.0 = worst case)
void generate_arrivals(
    std::vector<TaskParams>& tasks,
    core::Duration simulation_duration,
    std::mt19937& rng,
    double exec_ratio = 1.0);

// Convenience: generate complete scenario
ScenarioData generate_scenario(
    std::size_t num_tasks,
    double target_utilization,
    PeriodDistribution period_dist,
    core::Duration simulation_duration,
    std::mt19937& rng,
    double exec_ratio = 1.0);

} // namespace schedsim::io
