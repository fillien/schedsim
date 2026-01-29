#include <schedsim/io/scenario_generation.hpp>

#include <algorithm>
#include <cmath>

namespace schedsim::io {

using namespace schedsim::core;

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
    double min_period = period_dist.min.count();
    double max_period = period_dist.max.count();

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

        task_params.period = Duration{period};
        task_params.relative_deadline = Duration{period};  // Implicit deadline
        task_params.wcet = Duration{period * utilizations[idx]};

        tasks.push_back(std::move(task_params));
    }

    return tasks;
}

void generate_arrivals(
    std::vector<TaskParams>& tasks,
    Duration simulation_duration,
    std::mt19937& rng,
    double exec_ratio) {

    double sim_end = simulation_duration.count();

    for (auto& task : tasks) {
        task.jobs.clear();

        double period = task.period.count();
        double wcet = task.wcet.count();

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
                TimePoint{Duration{arrival}},
                Duration{duration}
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

} // namespace schedsim::io
