#include "scenario.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <random>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

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
                                    static_cast<uint16_t>(tid), utilization, period,
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

/**
 * @brief Helper function to print information to a specified output stream.
 *
 * @param out The output stream to which the information will be printed.
 * @param program_name The name of the program or module associated with the information.
 */
void print_helper(std::ostream& out, std::string program_name)
{
        out << "Usage: " << program_name << " [options]\n"
            << "Options:\n"
            << "  -h, --help          Display this help message\n"
            << "  -c, --cores N       Set the number of cores to N\n"
            << "  -t, --tasks N       Set the number of tasks to N\n"
            << "  -j, --jobs N        Set the number of jobs per task to N\n"
            << "  -u, --totalu U      Set the total utilization to U\n"
            << "  -o, --output FILE   Set the output file to FILE (must be a .json file)\n";
}

auto main(int argc, char* argv[]) -> int
{
        using namespace std::filesystem;

        const path DEFAULT_OUTPUT_FILE{"scenario.json"};
        path output_filepath{DEFAULT_OUTPUT_FILE};

        bool has_define_nb_cores{false};
        bool has_define_nb_tasks{false};
        bool has_define_nb_jobs{false};
        bool has_define_totalu{false};
        bool has_define_output_file{false};

        constexpr double MAX_PERIOD{100};
        int nb_cores{0};
        int nb_tasks{0};
        int nb_jobs_per_task{0};
        double total_utilization{0};

        const std::vector<std::string> args(argv + 1, argv + argc);

        if (args.empty()) { throw std::runtime_error("No input number of task to generate"); }

        for (auto arg = std::begin(args); arg != std::end(args); ++arg) {
                if (*arg == "-h" || *arg == "--help") {
                        print_helper(std::cout, argv[0]);
                        return EXIT_SUCCESS;
                }
                else if (*arg == "-c" || *arg == "--cores") {
                        if (has_define_nb_cores) {
                                throw std::runtime_error("Already defined a number of cores");
                        }
                        arg = std::next(arg);
                        nb_cores = std::atoi((*arg).c_str());
                        has_define_nb_cores = true;
                }
                else if (*arg == "-t" || *arg == "--tasks") {
                        if (has_define_nb_tasks) {
                                throw std::runtime_error("Already defined a number of tasks");
                        }
                        arg = std::next(arg);
                        nb_tasks = std::atoi((*arg).c_str());
                        has_define_nb_tasks = true;
                }
                else if (*arg == "-j" || *arg == "--jobs") {
                        if (has_define_nb_jobs) {
                                throw std::runtime_error(
                                    "Already defined a number of jobs per task");
                        }
                        arg = std::next(arg);
                        nb_jobs_per_task = std::atoi((*arg).c_str());
                        has_define_nb_jobs = true;
                }
                else if (*arg == "-u" || *arg == "--totalu") {
                        if (has_define_totalu) {
                                throw std::runtime_error("Already defined a total utilization");
                        }
                        arg = std::next(arg);
                        total_utilization = std::stod((*arg));
                        has_define_totalu = true;
                }
                else if (*arg == "-o" || *arg == "--output") {
                        if (has_define_output_file) {
                                throw std::runtime_error("Already defined a output file");
                        }
                        arg = std::next(arg);
                        output_filepath = *arg;
                        if (!output_filepath.has_filename() ||
                            output_filepath.extension() != ".json") {
                                throw std::runtime_error(
                                    std::string(output_filepath) + ": Not a JSON scenario file");
                        }
                        has_define_output_file = true;
                }
                else {
                        std::cerr << "Unknown argument" << std::endl;
                        return EXIT_FAILURE;
                }
        }

        if (!has_define_nb_cores || !has_define_nb_tasks || !has_define_nb_jobs ||
            !has_define_totalu || !has_define_output_file) {
                std::cerr << "Too few argument" << std::endl;
                return EXIT_FAILURE;
        }

        // Generate task set
        scenario::setting taskset{};
        taskset.nb_cores = nb_cores;
        taskset.tasks = generate_taskset(nb_tasks, MAX_PERIOD, total_utilization);

        // Generate jobs and arrivals for each task
        for (auto& gen_task : taskset.tasks) {
                generate_jobs(gen_task, nb_jobs_per_task);
        }

        // Write the scenario to output file
        scenario::write_file(output_filepath, taskset);

        return EXIT_SUCCESS;
}
