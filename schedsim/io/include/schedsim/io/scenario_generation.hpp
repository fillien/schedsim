#pragma once

#include <schedsim/io/scenario_loader.hpp>

#include <array>
#include <cstddef>
#include <random>
#include <vector>

namespace schedsim::io {

// Harmonic period set (microseconds) - all divide hyperperiod 25200
inline constexpr std::array<int, 10> HARMONIC_PERIODS_US{
    25200, 12600, 8400, 6300, 5040, 4200, 3600, 3150, 2800, 2520};
inline constexpr int HYPERPERIOD_US = 25200;

// UUniFast algorithm: generate n utilizations summing to U
std::vector<double> uunifast(std::size_t num_tasks, double target_utilization, std::mt19937& rng);

// UUniFast-Discard: respects per-task utilization bounds [umin, umax]
// Retries until all utilizations fall within bounds
// Throws std::runtime_error after 2B attempts if impossible
std::vector<double> uunifast_discard(
    std::size_t num_tasks,
    double target_utilization,
    double umin,
    double umax,
    std::mt19937& rng);

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

// Weibull job generation parameters
struct WeibullJobConfig {
    double success_rate{1.0};      // Percentile for WCET budget [0,1]
    double compression_rate{1.0};  // Min duration as fraction of WCET [0,1]
};

// Generate jobs with Weibull-distributed durations
// hyperperiod_jobs: number of jobs to generate (typically hyperperiod / period)
std::vector<JobParams> generate_weibull_jobs(
    core::Duration period,
    core::Duration wcet,
    std::size_t hyperperiod_jobs,
    const WeibullJobConfig& config,
    std::mt19937& rng);

// Pick random period from harmonic fixed set
core::Duration pick_harmonic_period(std::mt19937& rng);

// Full UUniFast-Discard-Weibull generation
// Task IDs start at 1 (legacy behavior)
ScenarioData generate_uunifast_discard_weibull(
    std::size_t num_tasks,
    double target_utilization,
    double umin,
    double umax,
    const WeibullJobConfig& config,
    std::mt19937& rng);

// Utility: merge two scenario task sets
// Reassigns sequential IDs starting at 1
ScenarioData merge_scenarios(const ScenarioData& a, const ScenarioData& b);

// Utility: build tasks from explicit utilization vector
// Uses harmonic periods and Weibull job generation
std::vector<TaskParams> from_utilizations(
    const std::vector<double>& utilizations,
    const WeibullJobConfig& config,
    std::mt19937& rng);

} // namespace schedsim::io
