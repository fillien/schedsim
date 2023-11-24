#include "scenario.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <ranges>
#include <string>
#include <vector>

// Function to generate a random double between min and max
auto getRandomDouble(double min, double max) -> double
{
        static std::mt19937_64 random_gen(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<double> dis(min, max);
        return dis(random_gen);
}

// Function to generate a random double using the UUnifast algorithm
auto uunifast(double totalUtilization, int numTasks) -> double
{
        double sumU = totalUtilization;
        double nextSumU{0};
        double u{0};

        for (int i = 1; i < numTasks; ++i) {
                nextSumU = sumU * pow(getRandomDouble(0, 1), 1.0 / (numTasks - i));
                u = sumU - nextSumU;
                sumU = nextSumU;
        }

        return sumU;
}

// Function to generate a random double between min and max with log-uniform distribution
auto get_random_log_uniform(double min, double max) -> double
{
        double logMin = log(min);
        double logMax = log(max);
        double logValue = getRandomDouble(logMin, logMax);
        return exp(logValue);
}

// Function to generate a task set with log-uniformly distributed periods
auto generate_taskset(int nb_tasks, double max_period, double total_utilization)
    -> std::vector<scenario::task>
{
        std::vector<scenario::task> taskset;
        double remaining_utilization = total_utilization;

        while (remaining_utilization > 0) {
                std::vector<double> utilizations;
                for (int i = 0; i < nb_tasks; ++i) {
                        double u = uunifast(remaining_utilization, nb_tasks - i);
                        utilizations.push_back(u);
                        remaining_utilization -= u;
                }

                // Discard task sets with insufficient utilization
                if (remaining_utilization <= 0) {
                        for (int i = 1; i <= nb_tasks; ++i) {
                                double utilization = utilizations[i - 1];
                                double period = get_random_log_uniform(1, max_period);
                                scenario::task new_task(i, utilization, period);
                                taskset.push_back(new_task);
                        }
                }
        }

        return taskset;
}

// Function to generate job execution times using a Weibull distribution
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

auto main(int argc, char* argv[]) -> int
{
        /// @todo Handle input errors

        const std::vector<std::string> args(argv + 1, argv + argc);

        if (args.empty()) { throw std::runtime_error("No input number of task to generate"); }

        // Number of tasks, maximum period, total utilization, number of jobs per task,
        // and parameters for the Weibull distribution
        int nb_tasks = std::stoi(args.at(0));
        int nb_jobs_per_task = 10; // Number of jobs per task
        double max_period = 100;
        double total_utilization = 0.8; // Total utilization for the task set

        // Generate task set
        scenario::setting taskset{};
        taskset.nb_cores = 2;
        taskset.tasks = generate_taskset(nb_tasks, max_period, total_utilization);

        // Generate jobs and arrivals for each task
        for (auto& j : taskset.tasks) {
                generate_jobs(j, nb_jobs_per_task);
        }

        scenario::write_file("sce.json", taskset);

        return EXIT_SUCCESS;
}
