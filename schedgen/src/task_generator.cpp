#include "task_generator.hpp"
#include <iterator>
#include <protocols/scenario.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

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
auto uunifast_discard(std::size_t nb_tasks, double total_utilization, double umax)
    -> std::vector<double>
{
        constexpr std::size_t DISCARD_LIMIT{1000};
        std::size_t discard_counter{0};
        std::uniform_real_distribution<double> distribution(0, 1);
        std::vector<double> utilizations;
        double sum_utilization = total_utilization;

        for (std::size_t i{1}; i < nb_tasks; ++i) {
                double new_rand{distribution(random_gen)};
                const double next_sum_utilization{
                    sum_utilization * std::pow(new_rand, 1.0 / static_cast<double>(nb_tasks - i))};

                if ((sum_utilization - next_sum_utilization) > umax) {
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

auto generate_jobs(std::vector<double>& durations, double period)
    -> std::vector<protocols::scenario::job>
{
        using namespace protocols::scenario;

        double time{0};
        std::vector<job> jobs;
        jobs.reserve(durations.size());

        for (const auto& duration : durations) {
                jobs.push_back(job{.arrival = time, .duration = duration});
                time += period;
        }

        return jobs;
}

auto generate_task(
    std::size_t tid,
    std::size_t nb_jobs,
    double success_rate,
    double compression_rate,
    double wcet,
    double task_period) -> protocols::scenario::task
{
        using namespace protocols::scenario;
        using std::ceil;

        assert(nb_jobs > 0);
        assert(success_rate >= 0 && success_rate <= 1);

        std::vector<double> durations(nb_jobs);

        std::ranges::generate(durations.begin(), durations.end(), [&]() {
                if (compression_rate == 1) { return wcet; }
                return bounded_weibull(compression_rate * wcet, wcet);
        });

        std::sort(durations.begin(), durations.end());

        const std::size_t index{static_cast<std::size_t>(ceil((nb_jobs - 1) * success_rate))};
        const double budget{durations.at(index)};

        std::shuffle(durations.begin(), durations.end(), random_gen);
        return task{
            .id = static_cast<std::size_t>(tid + 1),
            .utilization = budget / task_period,
            .period = task_period,
            .jobs = generate_jobs(durations, task_period)};
}

auto lcm(const std::vector<int>& nums) -> int
{
        return std::accumulate(nums.begin(), nums.end(), 1, std::lcm<int, int>);
}

auto generate_taskset(
    std::size_t nb_tasks,
    double total_utilization,
    double success_rate,
    double compression_rate) -> protocols::scenario::setting
{
        using namespace protocols::scenario;
        using std::round;

        constexpr double UMAX{1};

        if (total_utilization <= 0) {
                throw std::invalid_argument("Total utilization must be greater than 0");
        }

        if (success_rate < 0 || success_rate > 1) {
                throw std::invalid_argument("Success rate is not between 0 and 1");
        }

        // Step 1
        auto utilizations = uunifast_discard(nb_tasks, total_utilization, UMAX);
        /*std::vector<int> periods{25200, 12600, 8400, 6300, 5040, 4200, 3600, 3150, 2800, 2520};*/
        std::vector<int> periods{1, 2, 5, 10, 20, 50, 100, 200, 500, 1000};
        /*double hyperperiod{25200};*/
        double hyperperiod{1000};

        // Step 2
        std::vector<task> tasks;
        tasks.reserve(nb_tasks);

        for (size_t tid = 0; tid < nb_tasks; ++tid) {
                std::shuffle(std::begin(periods), std::end(periods), random_gen);
                const double util = utilizations.at(tid);
                const double period = static_cast<double>(periods.at(tid));
                const auto nb_jobs = hyperperiod / period;
                const double wcet = period * util;

                tasks.push_back(
                    generate_task(tid, nb_jobs, success_rate, compression_rate, wcet, period));
        }

        return setting{.tasks = tasks};
}
