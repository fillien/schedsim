#ifndef TASK_GENERATOR_HPP
#define TASK_GENERATOR_HPP

#include "scenario.hpp"
#include <cstddef>
#include <vector>

/**
 * @brief Generate a random double using the UUnifast algorithm.
 *
 * @param total_utilization The total utilization to be distributed among tasks.
 * @param nb_tasks The number of tasks to generate utilizations for.
 * @return A random double representing the assigned utilization for a task.
 */
auto uunifast(double total_utilization, int nb_tasks) -> double;

/**
 * @brief Function to generate job execution times using a Weibull distribution.
 *
 * @param task The task for which jobs are generated.
 * @param nb_job Number of jobs to generate.
 */
auto generate_jobs(std::vector<double> utilization) -> std::vector<scenario::job>;

/**
 * @brief Generate a task set with log-uniformly distributed periods.
 *
 * @param nb_tasks The number of tasks to generate in the task set.
 * @param max_period The maximum period allowed for tasks.
 * @param total_utilization The total utilization to distribute among tasks.
 * @return A vector of tasks representing the generated task set.
 */
auto generate_taskset(std::size_t nb_tasks, std::size_t nb_jobs, double total_utilization, double success_rate)
    -> std::vector<scenario::task>;

#endif
