#include <generators/uunifast_discard_weibull.hpp>
#include <protocols/scenario.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
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
auto uunifast_discard(
    std::size_t nb_tasks,
    double total_utilization,
    double umax,
    std::optional<std::pair<double, double>> a_special_need = std::nullopt) -> std::vector<double>
{
        std::uniform_real_distribution<double> distribution(0, 1);
        std::vector<double> utilizations;
        bool have_a_special_need = a_special_need.has_value();

        bool discard = false;
        do {
                utilizations.clear();
                double sum_utilization = total_utilization;
                discard = false;

                for (std::size_t i = 1; i < nb_tasks; ++i) {
                        double new_rand = distribution(random_gen);
                        double next_sum_utilization =
                            sum_utilization *
                            std::pow(new_rand, 1.0 / static_cast<double>(nb_tasks - i));
                        double utilization = sum_utilization - next_sum_utilization;

                        if (utilization > umax || (have_a_special_need &&
                                                   (a_special_need.value().first > utilization ||
                                                    utilization > a_special_need.value().second))) {
                                discard = true;
                                break;
                        }
                        if (have_a_special_need && (a_special_need.value().first <= utilization &&
                                                    utilization <= a_special_need.value().second)) {
                                have_a_special_need = false;
                        }

                        utilizations.push_back(utilization);
                        sum_utilization = next_sum_utilization;
                }

                if (!discard) {
                        double last_utilization = sum_utilization;
                        if (last_utilization > umax) { discard = true; }
                        else {
                                utilizations.push_back(last_utilization);
                        }
                }
        } while (discard);

        constexpr double UTIL_ROUND{0.01};
        double sum = std::accumulate(utilizations.begin(), utilizations.end(), 0.0);
        assert(std::abs(sum - total_utilization) < UTIL_ROUND);
        assert(std::ranges::all_of(utilizations, [umax](double u) { return u <= umax; }));
        assert(utilizations.size() == nb_tasks);

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

        /// TODO Need a constexpr std::exp function to be evaluated at compile time
        const double UPPER_BOUND{inversed_weibull_cdf(SHAPE, SCALE, UPPER_BOUND_QUANT)};
        std::weibull_distribution<> dist(SHAPE, SCALE);

        double res;
        do {
                auto distri = dist(random_gen);
                res = distri * ((max - min) / UPPER_BOUND) + min;
        } while (res < min || res > max);

        return res;
}

auto generate_jobs(std::vector<double>& durations, double period)
    -> std::vector<protocols::scenario::Job>
{
        using namespace protocols::scenario;

        double time{0};
        std::vector<Job> jobs;
        jobs.reserve(durations.size());

        for (const auto& duration : durations) {
                jobs.push_back(Job{.arrival = time, .duration = duration});
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
    double task_period) -> protocols::scenario::Task
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

        std::ranges::sort(durations);

        const std::size_t index{static_cast<std::size_t>(ceil((nb_jobs - 1) * success_rate))};
        const double budget{durations.at(index)};

        std::shuffle(durations.begin(), durations.end(), random_gen);
        return Task{
            .id = static_cast<std::size_t>(tid + 1),
            .utilization = budget / task_period,
            .period = task_period,
            .jobs = generate_jobs(durations, task_period)};
}

auto pick_period(const std::span<const int>& periods) -> int
{
        assert(!periods.empty());
        std::uniform_int_distribution<std::size_t> dis(0, periods.size() - 1);
        return periods[dis(random_gen)];
}
} // namespace

namespace generators {
auto uunifast_discard_weibull(
    std::size_t nb_tasks,
    double total_utilization,
    double umax,
    double success_rate,
    double compression_rate,
    const std::optional<std::pair<double, double>>& a_special_need) -> protocols::scenario::Setting
{
        using namespace protocols::scenario;
        using std::round;

        constexpr std::array<int, 10> PERIODS{
            25200, 12600, 8400, 6300, 5040, 4200, 3600, 3150, 2800, 2520};
        constexpr int HYPERPERIOD = PERIODS.at(0);
        constexpr double UTIL_ROUNDING{0.01};

        if (1 > nb_tasks || nb_tasks > std::numeric_limits<std::size_t>::max()) {
                throw std::invalid_argument(
                    "uunifast_discard_weibull: nb_tasks is out of bounds 1..max_size_t");
        }
        if (0 > total_utilization || total_utilization > std::numeric_limits<double>::max()) {
                throw std::invalid_argument(
                    "uunifast_discard_weibull: total utilization is out of bounds 0..max_double");
        }
        if (0 > umax || umax > 1) {
                throw std::invalid_argument("uunifast_discard_weibull: umax is out of bounds 0..1");
        }
        if (0 > success_rate || success_rate > 1) {
                throw std::invalid_argument(
                    "uunifast_discard_weibull: success_rate is out of bounds 0..1");
        }
        if (0 > compression_rate || compression_rate > 1) {
                throw std::invalid_argument(
                    "uunifast_discard_weibull: compression_rate is out of bounds 0..1");
        }
        if (static_cast<double>(nb_tasks) * umax < total_utilization) {
                throw std::invalid_argument("uunifast_discard_weibull: can't acheive the total "
                                            "utilization with those parameters");
        }

        double sum_of_utils = 0;
        std::vector<double> utilizations;
        while (std::abs(sum_of_utils - total_utilization) > UTIL_ROUNDING) {
                utilizations = uunifast_discard(nb_tasks, total_utilization, umax, a_special_need);
                sum_of_utils = std::ranges::fold_left(utilizations, 0.0, std::plus<>());
        }

        std::vector<Task> tasks;
        tasks.reserve(nb_tasks);

        for (size_t tid = 0; tid < nb_tasks; ++tid) {
                const int period = pick_period(PERIODS);
                const int nb_jobs = HYPERPERIOD / period;
                const double util = utilizations.at(tid);
                const double wcet = period * util;
                tasks.push_back(
                    generate_task(tid, nb_jobs, success_rate, compression_rate, wcet, period));
        }

        return Setting{.tasks = tasks};
}

auto generate_tasksets(
    std::string path,
    std::size_t nb_taskset,
    std::size_t nb_tasks,
    double total_utilization,
    double umax,
    double success_rate,
    double compression_rate,
    std::optional<std::pair<double, double>> a_special_need,
    std::size_t nb_cores) -> void
{
        std::filesystem::path output_path = path;
        if (!std::filesystem::exists(output_path) || !std::filesystem::is_directory(output_path)) {
                throw std::invalid_argument(
                    "generate_tasksets: output path does not exist or is not a directory");
        }

        // Use a queue to hold the taskset indices.  This decouples generation from writing.
        std::queue<std::size_t> taskset_indices;
        for (std::size_t i = 1; i <= nb_taskset; ++i) {
                taskset_indices.push(i);
        }

        // Mutex to protect access to the queue and potentially shared resources in
        // uunifast_discard_weibull/write_file
        std::mutex queue_mutex;

        std::vector<std::thread> threads;
        for (std::size_t i = 0; i < nb_cores; ++i) {
                threads.emplace_back([&taskset_indices,
                                      &queue_mutex,
                                      nb_tasks,
                                      total_utilization,
                                      umax,
                                      success_rate,
                                      compression_rate,
                                      a_special_need,
                                      output_path]() {
                        while (true) {
                                std::size_t index;
                                { // Scope for the lock
                                        std::lock_guard<std::mutex> lock(queue_mutex);
                                        if (taskset_indices.empty()) {
                                                break; // Queue is empty, thread can exit
                                        }
                                        index = taskset_indices.front();
                                        taskset_indices.pop();
                                }

                                // Generate the taskset *after* acquiring the index from the queue.
                                auto taskset = uunifast_discard_weibull(
                                    nb_tasks,
                                    total_utilization,
                                    umax,
                                    success_rate,
                                    compression_rate,
                                    a_special_need);

                                auto filename = std::to_string(index) + ".json";
                                std::filesystem::path filepath = output_path / filename;
                                protocols::scenario::write_file(filepath, taskset);
                        }
                });
        }

        for (auto& thr : threads) {
                thr.join();
        }
}

} // namespace generators
