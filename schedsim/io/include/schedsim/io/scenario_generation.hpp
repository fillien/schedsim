#pragma once

/// @file scenario_generation.hpp
/// @brief Programmatic task-set and job generation for scheduling experiments.
///
/// Provides UUniFast-based utilization splitting, harmonic period selection,
/// Weibull-distributed execution times, and convenience wrappers that produce
/// complete ScenarioData instances ready for simulation.
///
/// @ingroup io_generation

#include <schedsim/io/scenario_loader.hpp>

#include <array>
#include <cstddef>
#include <random>
#include <vector>

namespace schedsim::io {

/// @brief Harmonic period set (microseconds).
///
/// All ten periods divide the common hyperperiod HYPERPERIOD_US = 25200,
/// ensuring that the generated task sets have a bounded hyperperiod.
///
/// @ingroup io_generation
inline constexpr std::array<int, 10> HARMONIC_PERIODS_US{
    25200, 12600, 8400, 6300, 5040, 4200, 3600, 3150, 2800, 2520};

/// @brief Hyperperiod shared by all entries in HARMONIC_PERIODS_US (microseconds).
/// @ingroup io_generation
inline constexpr int HYPERPERIOD_US = 25200;

/// @brief UUniFast algorithm: generate @p num_tasks utilizations summing to
///        @p target_utilization.
///
/// Implements the unbiased random utilization-splitting algorithm by Bini and
/// Buttazzo.
///
/// @param num_tasks           Number of utilizations to generate.
/// @param target_utilization  Desired sum of all utilizations.
/// @param rng                 Mersenne Twister PRNG instance.
/// @return Vector of per-task utilizations (size == @p num_tasks).
///
/// @see uunifast_discard
std::vector<double> uunifast(std::size_t num_tasks, double target_utilization, std::mt19937& rng);

/// @brief UUniFast-Discard: utilization splitting with per-task bounds.
///
/// Repeatedly invokes UUniFast and discards sets where any utilization falls
/// outside [@p umin, @p umax]. Guarantees bounded individual utilizations at
/// the cost of additional retries.
///
/// @param num_tasks           Number of utilizations to generate.
/// @param target_utilization  Desired sum of all utilizations.
/// @param umin                Minimum per-task utilization (inclusive).
/// @param umax                Maximum per-task utilization (inclusive).
/// @param rng                 Mersenne Twister PRNG instance.
/// @return Vector of per-task utilizations within the requested bounds.
///
/// @throws std::runtime_error  After 2 billion failed attempts if the bounds
///                             are infeasible.
///
/// @see uunifast
std::vector<double> uunifast_discard(
    std::size_t num_tasks,
    double target_utilization,
    double umin,
    double umax,
    std::mt19937& rng);

/// @brief Configuration for the period distribution used by generate_task_set.
///
/// @ingroup io_generation
struct PeriodDistribution {
    core::Duration min{};             ///< Minimum period.
    core::Duration max{};             ///< Maximum period.
    bool log_uniform{true};           ///< If true, sample periods log-uniformly (common in RT literature).
};

/// @brief Generate task parameters with a target total utilization.
///
/// Splits @p target_utilization across @p num_tasks tasks using UUniFast,
/// samples periods from @p period_dist, and derives WCETs accordingly.
/// Does **not** generate job arrivals; call generate_arrivals afterwards.
///
/// @param num_tasks           Number of tasks to generate.
/// @param target_utilization  Desired total utilization.
/// @param period_dist         Period sampling configuration.
/// @param rng                 Mersenne Twister PRNG instance.
/// @return Vector of generated task parameters (no jobs yet).
///
/// @see generate_arrivals, generate_scenario
std::vector<TaskParams> generate_task_set(
    std::size_t num_tasks,
    double target_utilization,
    PeriodDistribution period_dist,
    std::mt19937& rng);

/// @brief Generate job arrivals for every task in @p tasks.
///
/// Fills each task's @c jobs vector with periodic arrivals spanning
/// @p simulation_duration. The actual execution demand of each job is
/// @p exec_ratio times the task's WCET.
///
/// @param tasks                Tasks whose job lists will be populated (modified in place).
/// @param simulation_duration  Total duration over which to generate arrivals.
/// @param rng                  Mersenne Twister PRNG instance.
/// @param exec_ratio           Ratio of actual execution time to WCET; 1.0 means
///                             worst-case execution on every job.
///
/// @see generate_task_set, generate_scenario
void generate_arrivals(
    std::vector<TaskParams>& tasks,
    core::Duration simulation_duration,
    std::mt19937& rng,
    double exec_ratio = 1.0);

/// @brief Generate a complete scenario (tasks + job arrivals) in one call.
///
/// Convenience wrapper that calls generate_task_set followed by
/// generate_arrivals and returns the result as a ScenarioData.
///
/// @param num_tasks            Number of tasks to generate.
/// @param target_utilization   Desired total utilization.
/// @param period_dist          Period sampling configuration.
/// @param simulation_duration  Duration for job arrival generation.
/// @param rng                  Mersenne Twister PRNG instance.
/// @param exec_ratio           Ratio of actual execution time to WCET.
/// @return A ready-to-use scenario.
///
/// @see generate_task_set, generate_arrivals, inject_scenario
ScenarioData generate_scenario(
    std::size_t num_tasks,
    double target_utilization,
    PeriodDistribution period_dist,
    core::Duration simulation_duration,
    std::mt19937& rng,
    double exec_ratio = 1.0);

/// @brief Configuration for Weibull-distributed job execution times.
///
/// Controls the shape of the execution-time distribution so that generated
/// workloads exhibit realistic variability rather than constant WCET.
///
/// @ingroup io_generation
/// @see generate_weibull_jobs
struct WeibullJobConfig {
    double success_rate{1.0};      ///< Percentile for WCET budget [0, 1].
    double compression_rate{1.0};  ///< Minimum duration as a fraction of WCET [0, 1].
};

/// @brief Generate jobs with Weibull-distributed execution durations.
///
/// Produces @p hyperperiod_jobs jobs spaced one @p period apart. Each job's
/// duration is drawn from a Weibull distribution parameterised by @p config
/// and clamped to [0, @p wcet].
///
/// @param period           Task period (inter-arrival spacing).
/// @param wcet             Worst-case execution time (upper clamp).
/// @param hyperperiod_jobs Number of jobs to generate.
/// @param config           Weibull distribution parameters.
/// @param rng              Mersenne Twister PRNG instance.
/// @return Vector of generated job parameters.
///
/// @see WeibullJobConfig, generate_uunifast_discard_weibull
std::vector<JobParams> generate_weibull_jobs(
    core::Duration period,
    core::Duration wcet,
    std::size_t hyperperiod_jobs,
    const WeibullJobConfig& config,
    std::mt19937& rng);

/// @brief Pick a random period from the harmonic fixed set.
///
/// Uniformly selects one of the ten entries in HARMONIC_PERIODS_US and
/// returns it as a @c core::Duration.
///
/// @param rng  Mersenne Twister PRNG instance.
/// @return A duration corresponding to one of the harmonic periods.
///
/// @see HARMONIC_PERIODS_US, HYPERPERIOD_US
core::Duration pick_harmonic_period(std::mt19937& rng);

/// @brief Full UUniFast-Discard + Weibull scenario generation.
///
/// Combines uunifast_discard for utilization splitting, harmonic period
/// selection, and generate_weibull_jobs for job generation into a single
/// call. Task IDs start at 1 (legacy convention).
///
/// @param num_tasks           Number of tasks.
/// @param target_utilization  Desired total utilization.
/// @param umin                Minimum per-task utilization.
/// @param umax                Maximum per-task utilization.
/// @param config              Weibull job configuration.
/// @param rng                 Mersenne Twister PRNG instance.
/// @return A complete scenario ready for injection.
///
/// @throws std::runtime_error  If UUniFast-Discard cannot find a valid split.
///
/// @see uunifast_discard, generate_weibull_jobs, inject_scenario
ScenarioData generate_uunifast_discard_weibull(
    std::size_t num_tasks,
    double target_utilization,
    double umin,
    double umax,
    const WeibullJobConfig& config,
    std::mt19937& rng);

/// @brief Merge two scenario task sets into one.
///
/// Concatenates the task lists from @p a and @p b and reassigns sequential
/// IDs starting at 1.
///
/// @param a  First scenario.
/// @param b  Second scenario.
/// @return Merged scenario with renumbered task IDs.
///
/// @see ScenarioData
ScenarioData merge_scenarios(const ScenarioData& a, const ScenarioData& b);

/// @brief Build task parameters from an explicit utilization vector.
///
/// Creates one task per entry in @p utilizations, assigns harmonic periods,
/// and generates Weibull-distributed jobs. Useful when utilizations are
/// obtained from an external source rather than UUniFast.
///
/// @param utilizations  Per-task utilizations.
/// @param config        Weibull job configuration.
/// @param rng           Mersenne Twister PRNG instance.
/// @return Vector of fully populated task parameters.
///
/// @see generate_weibull_jobs, pick_harmonic_period
std::vector<TaskParams> from_utilizations(
    const std::vector<double>& utilizations,
    const WeibullJobConfig& config,
    std::mt19937& rng);

} // namespace schedsim::io
