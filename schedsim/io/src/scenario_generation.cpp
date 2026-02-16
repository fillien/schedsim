#include <schedsim/io/scenario_generation.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace schedsim::io {

using namespace schedsim::core;

namespace {

// Inverse Weibull CDF for percentile calculation
constexpr double inverse_weibull_cdf(double shape, double scale, double percentile) {
    return scale * std::pow(-std::log(1.0 - percentile), 1.0 / shape);
}

// Generate bounded Weibull-distributed value in [min, max]
double bounded_weibull(double min_val, double max_val, std::mt19937& rng) {
    constexpr double SHAPE = 1.0;
    constexpr double SCALE = 2.0;
    constexpr double UPPER_BOUND_QUANT = 0.99;
    const double UPPER_BOUND = inverse_weibull_cdf(SHAPE, SCALE, UPPER_BOUND_QUANT);

    std::weibull_distribution<double> dist(SHAPE, SCALE);

    double result;
    do {
        double sample = dist(rng);
        result = sample * ((max_val - min_val) / UPPER_BOUND) + min_val;
    } while (result < min_val || result > max_val);

    return result;
}

} // anonymous namespace

std::vector<double> uunifast(std::size_t num_tasks, double target_utilization, std::mt19937& rng) {
    if (num_tasks == 0) {
        return {};
    }

    if (num_tasks == 1) {
        return {target_utilization};
    }

    std::vector<double> utilizations(num_tasks);
    double sum_u = target_utilization;
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (std::size_t idx = 0; idx < num_tasks - 1; ++idx) {
        // UUniFast formula: next_sum = sum_u * random^(1/(n-i-1))
        double exponent = 1.0 / static_cast<double>(num_tasks - idx - 1);
        double next_sum = sum_u * std::pow(dist(rng), exponent);
        utilizations[idx] = sum_u - next_sum;
        sum_u = next_sum;
    }
    utilizations[num_tasks - 1] = sum_u;

    return utilizations;
}

std::vector<double> uunifast_discard(
    std::size_t num_tasks,
    double target_utilization,
    double umin,
    double umax,
    std::mt19937& rng) {

    if (num_tasks == 0) {
        return {};
    }

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::vector<double> utilizations;
    constexpr int MAX_ATTEMPTS = 2'000'000'000;

    for (int attempts = 0; attempts < MAX_ATTEMPTS; ++attempts) {
        utilizations.clear();
        utilizations.reserve(num_tasks);
        double sum_u = target_utilization;
        bool discard = false;

        for (std::size_t idx = 0; idx < num_tasks - 1; ++idx) {
            double exponent = 1.0 / static_cast<double>(num_tasks - idx - 1);
            double next_sum = sum_u * std::pow(dist(rng), exponent);
            double util = sum_u - next_sum;

            if (util < umin || util > umax) {
                discard = true;
                break;
            }

            utilizations.push_back(util);
            sum_u = next_sum;
        }

        if (!discard) {
            // Check last utilization
            if (sum_u >= umin && sum_u <= umax) {
                utilizations.push_back(sum_u);
                return utilizations;
            }
        }
    }

    throw std::runtime_error(
        "uunifast_discard: cannot achieve target utilization with given parameters");
}

std::vector<TaskParams> generate_task_set(
    std::size_t num_tasks,
    double target_utilization,
    PeriodDistribution period_dist,
    std::mt19937& rng) {

    if (num_tasks == 0) {
        return {};
    }

    // Generate utilizations using UUniFast
    auto utilizations = uunifast(num_tasks, target_utilization, rng);

    // Generate periods
    double min_period = duration_to_seconds(period_dist.min);
    double max_period = duration_to_seconds(period_dist.max);

    std::vector<TaskParams> tasks;
    tasks.reserve(num_tasks);

    for (std::size_t idx = 0; idx < num_tasks; ++idx) {
        TaskParams task_params;
        task_params.id = idx;

        // Generate period
        double period;
        if (period_dist.log_uniform) {
            // Log-uniform: uniform in log space
            double log_min = std::log(min_period);
            double log_max = std::log(max_period);
            std::uniform_real_distribution<double> log_dist(log_min, log_max);
            period = std::exp(log_dist(rng));
        } else {
            // Uniform
            std::uniform_real_distribution<double> uniform_dist(min_period, max_period);
            period = uniform_dist(rng);
        }

        task_params.period = duration_from_seconds(period);
        task_params.relative_deadline = duration_from_seconds(period);  // Implicit deadline
        task_params.wcet = duration_from_seconds(period * utilizations[idx]);

        tasks.push_back(std::move(task_params));
    }

    return tasks;
}

void generate_arrivals(
    std::vector<TaskParams>& tasks,
    Duration simulation_duration,
    std::mt19937& rng,
    double exec_ratio) {

    double sim_end = duration_to_seconds(simulation_duration);

    for (auto& task : tasks) {
        task.jobs.clear();

        double period = duration_to_seconds(task.period);
        double wcet = duration_to_seconds(task.wcet);

        // Generate job arrivals at periodic intervals
        for (double arrival = 0.0; arrival < sim_end; arrival += period) {
            double duration;
            if (exec_ratio >= 1.0) {
                // Worst case: use full WCET
                duration = wcet;
            } else {
                // Random execution time up to exec_ratio * wcet
                std::uniform_real_distribution<double> exec_dist(
                    exec_ratio * 0.5 * wcet,  // Minimum 50% of target
                    exec_ratio * wcet);       // Maximum at target
                duration = exec_dist(rng);
            }

            task.jobs.push_back(JobParams{
                time_from_seconds(arrival),
                duration_from_seconds(duration)
            });
        }
    }
}

ScenarioData generate_scenario(
    std::size_t num_tasks,
    double target_utilization,
    PeriodDistribution period_dist,
    Duration simulation_duration,
    std::mt19937& rng,
    double exec_ratio) {

    ScenarioData scenario;
    scenario.tasks = generate_task_set(num_tasks, target_utilization, period_dist, rng);
    generate_arrivals(scenario.tasks, simulation_duration, rng, exec_ratio);
    return scenario;
}

Duration pick_harmonic_period(std::mt19937& rng) {
    std::uniform_int_distribution<std::size_t> dist(0, HARMONIC_PERIODS_US.size() - 1);
    int period_us = HARMONIC_PERIODS_US[dist(rng)];
    return duration_from_seconds(static_cast<double>(period_us) / 1'000'000.0);  // Convert us to seconds
}

std::vector<JobParams> generate_weibull_jobs(
    Duration period,
    Duration wcet,
    std::size_t hyperperiod_jobs,
    const WeibullJobConfig& config,
    std::mt19937& rng) {

    if (hyperperiod_jobs == 0) {
        return {};
    }

    double wcet_sec = duration_to_seconds(wcet);
    double period_sec = duration_to_seconds(period);

    // Generate durations with Weibull distribution
    std::vector<double> durations(hyperperiod_jobs);
    if (config.compression_rate >= 1.0) {
        // No compression: all durations are exactly wcet
        std::fill(durations.begin(), durations.end(), wcet_sec);
    } else {
        double min_duration = config.compression_rate * wcet_sec;
        for (auto& dur : durations) {
            dur = bounded_weibull(min_duration, wcet_sec, rng);
        }
    }

    // Sort to find budget at success_rate percentile
    std::sort(durations.begin(), durations.end());
    std::size_t budget_index = static_cast<std::size_t>(
        std::ceil(static_cast<double>(hyperperiod_jobs - 1) * config.success_rate));
    // Budget is not used here but could be useful for logging

    // Shuffle back to random order
    std::shuffle(durations.begin(), durations.end(), rng);

    // Generate jobs with sequential arrivals
    std::vector<JobParams> jobs;
    jobs.reserve(hyperperiod_jobs);
    double arrival_time = 0.0;
    for (std::size_t idx = 0; idx < hyperperiod_jobs; ++idx) {
        jobs.push_back(JobParams{
            time_from_seconds(arrival_time),
            duration_from_seconds(durations[idx])
        });
        arrival_time += period_sec;
    }

    return jobs;
}

ScenarioData generate_uunifast_discard_weibull(
    std::size_t num_tasks,
    double target_utilization,
    double umin,
    double umax,
    const WeibullJobConfig& config,
    std::mt19937& rng) {

    if (num_tasks == 0) {
        return {};
    }

    // Validate parameters
    if (umin > umax || umax > 1.0) {
        throw std::invalid_argument(
            "generate_uunifast_discard_weibull: umax must be in [umin, 1]");
    }
    if (umin < 0.0 || umin > umax) {
        throw std::invalid_argument(
            "generate_uunifast_discard_weibull: umin must be in [0, umax]");
    }
    if (static_cast<double>(num_tasks) * umin > target_utilization) {
        throw std::invalid_argument(
            "generate_uunifast_discard_weibull: num_tasks * umin > target_utilization");
    }
    if (target_utilization > static_cast<double>(num_tasks) * umax) {
        throw std::invalid_argument(
            "generate_uunifast_discard_weibull: target_utilization > num_tasks * umax");
    }
    if (config.success_rate < 0.0 || config.success_rate > 1.0) {
        throw std::invalid_argument(
            "generate_uunifast_discard_weibull: success_rate must be in [0, 1]");
    }
    if (config.compression_rate < 0.0 || config.compression_rate > 1.0) {
        throw std::invalid_argument(
            "generate_uunifast_discard_weibull: compression_rate must be in [0, 1]");
    }

    // Generate utilizations
    auto utilizations = uunifast_discard(num_tasks, target_utilization, umin, umax, rng);

    // Generate tasks
    ScenarioData scenario;
    scenario.tasks.reserve(num_tasks);

    for (std::size_t idx = 0; idx < num_tasks; ++idx) {
        Duration period = pick_harmonic_period(rng);
        double util = utilizations[idx];
        Duration wcet = scale_duration(period, util);

        int period_us = static_cast<int>(duration_to_seconds(period) * 1'000'000.0);
        std::size_t num_jobs = static_cast<std::size_t>(HYPERPERIOD_US / period_us);

        auto jobs = generate_weibull_jobs(period, wcet, num_jobs, config, rng);

        TaskParams task;
        task.id = idx + 1;  // Task IDs start at 1 (legacy behavior)
        task.period = period;
        task.relative_deadline = period;  // Implicit deadline
        task.wcet = wcet;
        task.jobs = std::move(jobs);

        scenario.tasks.push_back(std::move(task));
    }

    return scenario;
}

ScenarioData merge_scenarios(const ScenarioData& first, const ScenarioData& second) {
    ScenarioData result;
    result.tasks.reserve(first.tasks.size() + second.tasks.size());

    uint64_t next_id = 1;  // IDs start at 1

    for (const auto& task : first.tasks) {
        TaskParams new_task = task;
        new_task.id = next_id++;
        result.tasks.push_back(std::move(new_task));
    }

    for (const auto& task : second.tasks) {
        TaskParams new_task = task;
        new_task.id = next_id++;
        result.tasks.push_back(std::move(new_task));
    }

    return result;
}

std::vector<TaskParams> from_utilizations(
    const std::vector<double>& utilizations,
    const WeibullJobConfig& config,
    std::mt19937& rng) {

    if (utilizations.empty()) {
        return {};
    }

    // Validate
    for (double util : utilizations) {
        if (util < 0.0 || util > 1.0) {
            throw std::invalid_argument(
                "from_utilizations: each utilization must be in [0, 1]");
        }
    }
    if (config.success_rate < 0.0 || config.success_rate > 1.0) {
        throw std::invalid_argument(
            "from_utilizations: success_rate must be in [0, 1]");
    }
    if (config.compression_rate < 0.0 || config.compression_rate > 1.0) {
        throw std::invalid_argument(
            "from_utilizations: compression_rate must be in [0, 1]");
    }

    std::vector<TaskParams> tasks;
    tasks.reserve(utilizations.size());

    for (std::size_t idx = 0; idx < utilizations.size(); ++idx) {
        Duration period = pick_harmonic_period(rng);
        double util = utilizations[idx];
        Duration wcet = scale_duration(period, util);

        int period_us = static_cast<int>(duration_to_seconds(period) * 1'000'000.0);
        std::size_t num_jobs = static_cast<std::size_t>(HYPERPERIOD_US / period_us);

        auto jobs = generate_weibull_jobs(period, wcet, num_jobs, config, rng);

        TaskParams task;
        task.id = idx + 1;  // Task IDs start at 1
        task.period = period;
        task.relative_deadline = period;
        task.wcet = wcet;
        task.jobs = std::move(jobs);

        tasks.push_back(std::move(task));
    }

    return tasks;
}

} // namespace schedsim::io
