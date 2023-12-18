#include "task_generator.hpp"
#include "scenario.hpp"

#include <chrono>
#include <cmath>
#include <random>
#include <stdexcept>

namespace {
using hr_clk = std::chrono::high_resolution_clock;
static std::mt19937_64 random_gen(hr_clk::now().time_since_epoch().count());

/**
 * @brief Generate a random double within the specified range.
 *
 * @param min The minimum value of the range.
 * @param max The maximum value of the range.
 * @return A random double within the specified range.
 */
auto get_random_double(double min, double max) -> double
{
        std::uniform_real_distribution<double> dis(min, max);
        return dis(random_gen);
}

/**
 * @brief Generates a set of task utilizations using the UUniFast discard algorithm.
 *
 * @param nb_tasks The number of tasks to generate utilizations for.
 * @param total_utilization The total system utilization to be distributed
 *                         among the generated tasks.
 *
 * @return A vector of doubles representing the generated task utilizations.
 */
auto uunifast_discard(std::size_t nb_tasks, double total_utilization) -> std::vector<double>
{
        // From "Techniques For The Synthesis Of Multiprocessor Tasksets"
        constexpr std::size_t DISCARD_LIMIT{1000};
        std::size_t discard_counter{0};
        std::uniform_real_distribution<double> distribution(0, 1);
        std::vector<double> utilizations;
        double sum_utilization = total_utilization;

        for (std::size_t i{1}; i < nb_tasks; ++i) {
                double new_rand{distribution(random_gen)};
                double next_sum_utilization{
                    sum_utilization * std::pow(new_rand, 1.0 / static_cast<double>(nb_tasks - i))};

                if ((sum_utilization - next_sum_utilization) > 1) {
                        if (discard_counter > DISCARD_LIMIT) {
                                throw std::runtime_error("The utilization generation has exceeded "
                                                         "the limit of rejected task sets");
                        }
                        discard_counter++;
                        i = 0;
                        utilizations.clear();
                        continue;
                }

                utilizations.push_back(sum_utilization - next_sum_utilization);
                sum_utilization = next_sum_utilization;
        }
        utilizations.push_back(sum_utilization);

        return utilizations;
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
        return exp(get_random_double(log(min), log(max)));
}

} // namespace

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
        for (std::size_t i = 0; i < task.jobs.size(); ++i) {
                next_arrival += uniform_dis(generator);
                task.jobs.at(i).arrival = next_arrival;
                task.jobs.at(i).duration = weibull_dis(generator);
                next_arrival += task.period;
        }
}

/**
 * @brief Generate a task set with log-uniformly distributed periods.
 *
 * @param nb_tasks The number of tasks to generate in the task set.
 * @param max_period The maximum period allowed for tasks.
 * @param total_utilization The total utilization to distribute among tasks.
 * @return A vector of tasks representing the generated task set.
 */
auto generate_taskset(std::size_t nb_tasks, double max_period, double total_utilization)
    -> std::vector<scenario::task>
{
        if (total_utilization <= 0) {
                throw std::invalid_argument("Total utilization must be greater than 0");
        }

        std::vector<scenario::task> taskset;
        auto utilizations = uunifast_discard(nb_tasks, total_utilization);

        for (std::size_t tid{0}; tid < nb_tasks; ++tid) {
                scenario::task new_task{
                    .id = static_cast<uint16_t>(tid + 1),
                    .utilization = utilizations.at(tid),
                    .period = get_random_log_uniform(1, max_period),
                    .jobs = std::vector<scenario::job>{}};
                taskset.push_back(new_task);
        }

        return taskset;
}
