#include "task_generator.hpp"
#include "scenario.hpp"

#include <chrono>
#include <cmath>
#include <random>
#include <stdexcept>

/**
 * @brief Generate a random double within the specified range.
 *
 * @param min The minimum value of the range.
 * @param max The maximum value of the range.
 * @return A random double within the specified range.
 */
auto get_random_double(double min, double max) -> double
{
        static std::mt19937_64 random_gen(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<double> dis(min, max);
        return dis(random_gen);
}

/**
 * @brief Generate a random double using the UUnifast algorithm.
 *
 * @param total_utilization The total utilization to be distributed among tasks.
 * @param nb_tasks The number of tasks to generate utilizations for.
 * @return A random double representing the assigned utilization for a task.
 */
auto uunifast(double total_utilization, int nb_tasks) -> double
{
        double sum_utilization = total_utilization;
        double next_sum_utilization{0};
        double utilization{0};

        for (int i = 1; i < nb_tasks; ++i) {
                next_sum_utilization =
                    sum_utilization * pow(get_random_double(0, 1), 1.0 / (nb_tasks - i));
                utilization = sum_utilization - next_sum_utilization;
                sum_utilization = next_sum_utilization;
        }

        return sum_utilization;
}

/**
 * @brief Generate a random number from a log-uniform distribution.
 *
 * @param min The minimum value of the log-uniform distribution.
 * @param max The maximum value of the log-uniform distribution.
 * @return A random number from the log-uniform distribution within the specified range.
 */
auto get_random_log_uniform(double min, double max) -> double
{
        double log_min = log(min);
        double log_max = log(max);
        double log_value = get_random_double(log_min, log_max);
        return exp(log_value);
}

/**
 * @brief Generate a task set with log-uniformly distributed periods.
 *
 * @param nb_tasks The number of tasks to generate in the task set.
 * @param max_period The maximum period allowed for tasks.
 * @param total_utilization The total utilization to distribute among tasks.
 * @return A vector of tasks representing the generated task set.
 */
auto generate_taskset(int nb_tasks, double max_period, double total_utilization)
    -> std::vector<scenario::task>
{
        if (total_utilization <= 0) {
                throw std::invalid_argument("Total utilization must be greater than 0");
        }

        std::vector<scenario::task> taskset;
        double remaining_utilization = total_utilization;

        while (remaining_utilization > 0) {
                std::vector<double> utilizations;
                for (int i = 0; i < nb_tasks; ++i) {
                        double utilization = uunifast(remaining_utilization, nb_tasks - i);
                        utilizations.push_back(utilization);
                        remaining_utilization -= utilization;
                }

                // Discard task sets with insufficient utilization
                if (remaining_utilization <= 0) {
                        for (int tid = 1; tid <= nb_tasks; ++tid) {
                                double utilization = utilizations[tid - 1];
                                double period = get_random_log_uniform(1, max_period);
                                scenario::task new_task{
                                    static_cast<uint16_t>(tid),
                                    utilization,
                                    period,
                                    std::vector<scenario::job>{}};
                                taskset.push_back(new_task);
                        }
                }
        }

        return taskset;
}

/**
 * @brief Function to generate job execution times using a Weibull distribution.
 *
 * @param task The task for which jobs are generated.
 * @param nb_job Number of jobs to generate.
 */
void generate_jobs(scenario::task& task, int nb_job)
{
        std::default_random_engine generator;
        std::weibull_distribution<double> weibull_dis(3, task.period * task.utilization);
        std::uniform_real_distribution<double> uniform_dis(0, task.period);

        double next_arrival{0};
        task.jobs.resize(nb_job);
        for (size_t i = 0; i < task.jobs.size(); ++i) {
                next_arrival += uniform_dis(generator);
                task.jobs.at(i).arrival = next_arrival;
                task.jobs.at(i).duration = weibull_dis(generator);
                next_arrival += task.period;
        }
}
