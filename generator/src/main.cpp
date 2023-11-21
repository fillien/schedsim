#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <ranges>
#include <string>
#include <vector>

#include "yaml-cpp/emittermanip.h"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/yaml.h"

struct job {
        double arrival;
        double duration;
};

struct Task {
        int id;
        double utilization;    // Utilization factor
        double period;         // Period of the task
        double deadline;       // Implicit deadline (equal to the period in this example)
        std::vector<job> jobs; // Jobs of the task

        // Constructor
        Task(int i, double u, double p) : id(i), utilization(u), period(p), deadline(p) {}
};

// Function to generate a random double between min and max
auto getRandomDouble(double min, double max) -> double
{
        static std::mt19937_64 random_gen;
        std::uniform_real_distribution<double> dis(min, max);
        return dis(random_gen);
}

// Function to generate a random double using the UUnifast algorithm
auto uunifast(double totalUtilization, int numTasks) -> double
{
        double sumU = totalUtilization;
        double nextSumU;
        double u;

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
    -> std::vector<Task>
{
        std::vector<Task> taskset;
        double remainingUtilization = total_utilization;

        while (remainingUtilization > 0) {
                std::vector<double> utilizations;
                for (int i = 0; i < nb_tasks; ++i) {
                        double u = uunifast(remainingUtilization, nb_tasks - i);
                        utilizations.push_back(u);
                        remainingUtilization -= u;
                }

                // Discard task sets with insufficient utilization
                if (remainingUtilization <= 0) {
                        for (int i = 1; i <= nb_tasks; ++i) {
                                double utilization = utilizations[i - 1];
                                double period = get_random_log_uniform(1, max_period);
                                Task task(i, utilization, period);
                                taskset.push_back(task);
                        }
                }
        }

        return taskset;
}

// Function to generate job execution times using a Weibull distribution
void generate_jobs(Task& task, int nb_job)
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

// Function to print the generated task set along with jobs and arrivals
void printTaskSet(const std::vector<Task>& taskSet)
{
        std::cout << "Task Set:" << std::endl;
        std::cout << "ID\tUtilization\tPeriod\tDeadline\tJobs\tArrivals" << std::endl;
        for (const auto& task : taskSet) {
                std::cout << task.id << "\t" << task.utilization << "\t\t" << task.period << "\t"
                          << task.deadline << "\t\t";
                for (size_t i = 0; i < task.jobs.size(); ++i) {
                        std::cout << task.jobs.at(i).duration << " (" << task.jobs.at(i).arrival
                                  << ") ";
                }
                std::cout << std::endl;
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
        double max_period = 100;
        double total_utilization = 0.8; // Total utilization for the task set
        int nb_jobs_per_task = 10;      // Number of jobs per task

        // Generate task set
        std::vector<Task> taskset = generate_taskset(nb_tasks, max_period, total_utilization);

        // Generate jobs and arrivals for each task
        for (auto& j : taskset) {
                generate_jobs(j, nb_jobs_per_task);
        }

        // Print the generated task set along with jobs and arrivals
        // printTaskSet(taskset);

	constexpr int PRECISION{4};
	
        YAML::Emitter out;

        out << YAML::BeginMap;
        out << YAML::Key << "cores";
        out << YAML::Value << 2;

        out << YAML::Key << "tasks";
        out << YAML::BeginSeq;

        for (auto& task : taskset) {
                out << YAML::BeginMap;
                out << YAML::Key << "id";
                out << YAML::Value << task.id;
                out << YAML::Key << "period";
                out << YAML::Value << YAML::Precision(PRECISION) << task.period;
                out << YAML::Key << "utilization";
                out << YAML::Value << YAML::Precision(PRECISION) << task.utilization;
                out << YAML::Key << "jobs";

                out << YAML::BeginSeq;
                for (auto& j : task.jobs) {
                        out << YAML::BeginMap;
                        out << YAML::Key << "arrival";
                        out << YAML::Value << YAML::Precision(PRECISION) << j.arrival;
                        out << YAML::Key << "duration";
                        out << YAML::Value << YAML::Precision(PRECISION) << j.duration;
                        out << YAML::EndMap;
                }
                out << YAML::EndSeq;

                out << YAML::EndMap;
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::cout << out.c_str() << std::endl;

        return EXIT_SUCCESS;
}
