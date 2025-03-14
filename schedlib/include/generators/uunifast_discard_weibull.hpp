#ifndef TASK_GENERATOR_HPP
#define TASK_GENERATOR_HPP

#include <cstddef>
#include <optional>
#include <protocols/scenario.hpp>

namespace generators {
/**
 * @brief Generates a set of tasks (taskset) for a given scenario, each with a specific utilization
 * and number of jobs.
 *
 * This function distributes the total utilization and total number of jobs across the specified
 * number of tasks. Each task's utilization and number of jobs are determined proportionally to its
 * share of the total utilization. The function ensures that the generated taskset is feasible for
 * the given number of cores and fits the required total utilization and success rate. It's often
 * used in scheduling simulations where tasks need to be distributed across multiple processing
 * units.
 *
 * @param nb_cores The number of processor cores available.
 * @param nb_tasks The number of tasks to generate.
 * @param total_nb_jobs The total number of jobs to be distributed among all tasks.
 * @param total_utilization The total utilization to be distributed among all tasks.
 * @param success_rate The rate at which jobs are expected to successfully complete within their
 * period.
 * @return A 'setting' structure from the 'scenario' namespace, containing the number of cores and a
 * vector of generated tasks.
 *
 * @exception std::invalid_argument Thrown if the total utilization is less than or equal to 0.
 *
 * @note This function internally calls generate_task() for each task to be generated.
 */
auto uunifast_discard_weibull(
    std::size_t nb_tasks,
    double total_utilization,
    double umax,
    double success_rate,
    double compression_rate,
    std::optional<std::pair<double, double>> a_special_need) -> protocols::scenario::Setting;
} // namespace generators

#endif
