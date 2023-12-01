#include "scenario.hpp"
#include "task_generator.hpp"

#include <iostream>

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

        /// Parse CLI arguments
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
