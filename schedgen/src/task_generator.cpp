#include "task_generator.hpp"
#include "scenario.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
using hr_clk = std::chrono::high_resolution_clock;
static std::mt19937_64 random_gen(hr_clk::now().time_since_epoch().count());

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

constexpr auto inversed_weibull_cdf(double shape, double scale, double percentile) -> double
{
        return scale * std::pow(-std::log(1 - percentile), 1 / shape);
}

auto bounded_weibull(double min, double max) -> double
{
        assert(min > 0);
        assert(max > min);
        constexpr double SHAPE{1};
        constexpr double SCALE{2};
        constexpr double UPPER_BOUND_QUANT{0.9999};

        // Need a constexpr std::exp function to be evaluated at compile time
        const double UPPER_BOUND{inversed_weibull_cdf(SHAPE, SCALE, UPPER_BOUND_QUANT)};
        std::weibull_distribution<> dist(SHAPE, SCALE);

        const auto distri = dist(random_gen);

        auto res = distri * ((max - min) / UPPER_BOUND) + min;
        assert(res > min);
        assert(res < max);
        return res;
}
} // namespace

/**
 * @brief Function to generate job execution times using a Weibull distribution.
 *
 * @param task The task for which jobs are generated.
 * @param nb_job Number of jobs to generate.
 */
auto generate_jobs(std::vector<double> durations) -> std::vector<scenario::job>
{
        std::default_random_engine generator;

        double next_arrival{0};
/*
        for (std::size_t i = 0; i < task.jobs.size(); ++i) {
                next_arrival += uniform_dis(generator);
                task.jobs.at(i).arrival = next_arrival;
                task.jobs.at(i).duration = weibull_dis(generator);
                next_arrival += task.period;
        }
*/
        return std::vector<scenario::job>(1);
}

/**
 * @brief Generate a task set with log-uniformly distributed periods.
 *
 * @param nb_tasks The number of tasks to generate in the task set.
 * @param max_period The maximum period allowed for tasks.
 * @param total_utilization The total utilization to distribute among tasks.
 * @return A vector of tasks representing the generated task set.

 */
auto generate_taskset(
    std::size_t nb_tasks, std::size_t nb_jobs, double total_utilization, double success_rate)
    -> std::vector<scenario::task>
{
        // TODO Fix the difference between the number of jobs generated and the number of jobs
        // asked.

        if (total_utilization <= 0) {
                throw std::invalid_argument("Total utilization must be greater than 0");
        }

        std::vector<scenario::task> taskset;
        auto utilizations = uunifast_discard(nb_tasks, total_utilization);

        for (const auto& uti : utilizations) {
                std::cout << uti << ' ';
        }
        std::cout << std::endl;

        std::size_t sum_jobs{0};

        for (std::size_t tid{0}; tid < nb_tasks; ++tid) {
                std::vector<double> durations(
			std::round(nb_jobs * utilizations.at(tid) * 1 / total_utilization));
                sum_jobs += durations.size();

		if (durations.size() == 0) {
			continue;
		}

                std::ranges::generate(
			durations.begin(), durations.end(), []() { return bounded_weibull(20, 60); });

                std::sort(durations.begin(), durations.end());

                std::cout << '[';
                for (const auto& dur : durations) {
                        std::cout << dur << ' ';
                }
                std::cout << ']' << std::endl;

		std::size_t index = std::ceil((durations.size()-1) * success_rate);
		std::cout << "index: " << index << std::endl;
                double period{durations.at(index)};

                std::cout << "period : " << period << std::endl;

                scenario::task new_task{
			.id = static_cast<uint16_t>(tid + 1),
			.utilization = utilizations.at(tid),
			.period = period,
			.jobs = generate_jobs(durations)};
                taskset.push_back(new_task);

		std::cout << std::endl;
        }

        std::cout << "input nb_jobs: " << nb_jobs << std::endl;
        std::cout << "output nb_jobs: " << sum_jobs << std::endl;

        return taskset;
}
