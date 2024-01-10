#ifndef TASK_GENERATOR_HPP
#define TASK_GENERATOR_HPP

#include "scenario.hpp"
#include <cstddef>
#include <vector>

/**
 * @brief Generates a sequence of jobs with specified durations and period.
 *
 * This function creates a list of jobs, each with a specific duration and arrival time. The arrival
 * times are determined by a fixed period, meaning each subsequent job arrives exactly one period
 * after the previous one. It's designed for scenarios where tasks are executed periodically, and
 * their durations are predetermined or estimated.
 *
 * @param durations A vector of doubles, where each element represents the duration of a job.
 * @param period A double value indicating the time between the start of consecutive jobs.
 * @return A std::vector of 'job' from the 'scenario' namespace, each containing the scheduled time
 * of arrival and the duration for the job.
 */
auto generate_jobs(std::vector<double> durations, double period) -> std::vector<scenario::job>;

/**
 * @brief Generates a task with a set of jobs based on utilization and success rate.
 *
 * This function creates a single task with multiple jobs, each determined by a Weibull distribution
 * within given bounds. It calculates the period for each job based on the desired success rate and
 * then sorts the durations. The task's period is set to ensure a certain portion of jobs
 * (determined by the success rate) complete within their expected durations. The function ensures
 * that jobs are created periodically and are used in scenarios simulating task executions with
 * varying durations and reliability requirements.
 *
 * @param tid The task identifier, which is incremented before setting.
 * @param utilization The utilization factor of the task.
 * @param nb_jobs The number of jobs to generate for this task.
 * @param success_rate The rate at which jobs are expected to successfully complete within their
 * period.
 * @return A 'task' structure from the 'scenario' namespace, containing the task's ID, utilization,
 * period, and a vector of jobs.
 *
 * @note Assumes nb_jobs is greater than 0 and that the success_rate is a valid percentage.
 */
auto generate_task(
    std::size_t nb_tasks, std::size_t nb_jobs, double total_utilization, double success_rate)
    -> scenario::task;

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
auto generate_taskset(
    std::size_t nb_tasks, std::size_t nb_jobs, double total_utilization, double success_rate)
    -> scenario::setting;

#endif
