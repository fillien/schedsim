#ifndef TASK_GENERATOR_HPP
#define TASK_GENERATOR_HPP

#include "scenario.hpp"

/**
 * @brief Generate a random double within the specified range.
 *
 * @param min The minimum value of the range.
 * @param max The maximum value of the range.
 * @return A random double within the specified range.
 */
auto get_random_double(double min, double max) -> double;

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
void generate_jobs(scenario::task& task, int nb_job);

/**
 * @brief Generate a task set with log-uniformly distributed periods.
 *
 * @param nb_tasks The number of tasks to generate in the task set.
 * @param max_period The maximum period allowed for tasks.
 * @param total_utilization The total utilization to distribute among tasks.
 * @return A vector of tasks representing the generated task set.
 */
auto generate_taskset(std::size_t nb_tasks, double max_period, double total_utilization)
    -> std::vector<scenario::task>;

#endif
