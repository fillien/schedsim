#include "task_generator.hpp"
#include <protocols/scenario.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
using hr_clk = std::chrono::high_resolution_clock;
static std::mt19937_64 random_gen(hr_clk::now().time_since_epoch().count());

/**
 * @brief Generates a vector of utilizations for a given number of tasks such that their total
 * equals the specified total utilization.
 *
 * This function implements the UUniFast algorithm, which is designed to uniformly distribute total
 * utilization among a given number of tasks. The algorithm ensures that the sum of generated
 * utilizations closely matches the specified total utilization. It's commonly used in real-time
 * systems to generate task utilizations for simulation and testing purposes. The function may
 * discard and retry the generation process if the utilizations deviate significantly, ensuring a
 * valid and uniform distribution.
 *
 * @param nb_tasks The number of tasks for which to generate utilizations.
 * @param total_utilization The total utilization to be distributed among all tasks.
 * @return A vector of double values, each representing the utilization for a task.
 *
 * @exception std::runtime_error Thrown if the function exceeds the limit of discarded task sets due
 * to unrealistic utilization values.
 *
 * @note The function will iterate and attempt to generate a uniform distribution of utilizations.
 * It may discard the current set and retry if the generated values are not realistic or feasible.
 */
auto uunifast_discard(std::size_t nb_tasks, double total_utilization) -> std::vector<double>
{
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
        constexpr double UPPER_BOUND_QUANT{0.99};

        // Need a constexpr std::exp function to be evaluated at compile time
        const double UPPER_BOUND{inversed_weibull_cdf(SHAPE, SCALE, UPPER_BOUND_QUANT)};
        std::weibull_distribution<> dist(SHAPE, SCALE);

        double res;
        do {
                auto distri = dist(random_gen);
                res = distri * ((max - min) / UPPER_BOUND) + min;
        } while (res < min || res > max);

        return res;
}
} // namespace

auto generate_jobs(std::vector<double> durations, double period)
    -> std::vector<protocols::scenario::job>
{
        using namespace protocols::scenario;
        std::vector<job> jobs;
        jobs.reserve(durations.size());

        double next_arrival = 0;
        for (const auto& duration : durations) {
                jobs.push_back(job{next_arrival, duration});
                next_arrival += period;
        }

        return jobs;
}

auto generate_task(std::size_t tid, double utilization, std::size_t nb_jobs, double success_rate)
    -> protocols::scenario::task
{
        using namespace protocols::scenario;
        using std::ceil;

        assert(nb_jobs > 0);
        assert(success_rate >= 0 && success_rate <= 1);

        constexpr double PERIOD_MIN{20};
        constexpr double PERIOD_MAX{60};
        std::vector<double> durations(nb_jobs);

        std::ranges::generate(durations.begin(), durations.end(), []() {
                return bounded_weibull(PERIOD_MIN, PERIOD_MAX);
        });

        std::sort(durations.begin(), durations.end());

        const size_t index{static_cast<std::size_t>(ceil((nb_jobs - 1) * success_rate))};
        const double period{durations.at(index)};
        return task{
            .id = static_cast<std::size_t>(tid + 1),
            .utilization = utilization,
            .period = period,
            .jobs = generate_jobs(durations, period)};
}

auto generate_taskset(
    std::size_t nb_tasks, std::size_t total_nb_jobs, double total_utilization, double success_rate)
    -> protocols::scenario::setting
{
        using namespace protocols::scenario;
        using std::round;

        // TODO Fix the difference between the number of jobs generated and the number of jobs
        // asked.

        if (total_utilization <= 0) {
                throw std::invalid_argument("Total utilization must be greater than 0");
        }

        if (success_rate < 0 || success_rate > 1) {
                throw std::invalid_argument("Success rate is not between 0 and 1");
        }

        auto utilizations = uunifast_discard(nb_tasks, total_utilization);

        std::vector<task> tasks;
        tasks.reserve(nb_tasks);
        for (size_t tid = 0; tid < nb_tasks; ++tid) {
                const double task_util = utilizations.at(tid);
                const auto nb_jobs = static_cast<std::size_t>(
                    round(static_cast<double>(total_nb_jobs) * task_util / total_utilization));

                if (nb_jobs == 0) { continue; }

                tasks.push_back(generate_task(tid, task_util, nb_jobs, success_rate));
        }

        return setting{.tasks = tasks};
}
