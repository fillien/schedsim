#include "scenario.hpp"
#include "task_generator.hpp"

#include <cstdlib>
#include <cxxopts.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <typeinfo>

auto main(int argc, char* argv[]) -> int
{
        using namespace std::filesystem;

        constexpr double MAX_PERIOD{100};
        const path DEFAULT_OUTPUT_FILE{"scenario.json"};

        path output_filepath{DEFAULT_OUTPUT_FILE};
        int nb_cores{0};
        int nb_tasks{0};
        int nb_jobs_per_task{0};
        double total_utilization{0};

        cxxopts::Options options("generator", "Generate task for monocore and multicore systems");
        options.add_options()("h,help", "Helper")(
            "c,cores", "Number of cores", cxxopts::value<int>())(
            "t,tasks", "Number of tasks to generate", cxxopts::value<int>())(
            "j,jobs", "Number of jobs per tasks", cxxopts::value<int>())(
            "u,totalu", "Total utilization of the taskset", cxxopts::value<double>())(
            "o,output", "Output file to write the scenario", cxxopts::value<std::string>());

        try {
                const auto cli = options.parse(argc, argv);
                const bool mandatory_args = cli.count("cores") && cli.count("tasks") &&
                                            cli.count("jobs") && cli.count("totalu");

                if (cli.arguments().empty()) {
                        std::cerr << options.help() << std::endl;
                        return EXIT_FAILURE;
                }

                if (cli.count("help")) {
                        std::cout << options.help() << std::endl;
                        return EXIT_SUCCESS;
                }

                if (!mandatory_args) {
                        std::cerr << "Missing arguments" << std::endl;
                        return EXIT_FAILURE;
                }

                if (cli.count("cores")) { nb_cores = cli["cores"].as<int>(); }
                if (cli.count("tasks")) { nb_tasks = cli["tasks"].as<int>(); }
                if (cli.count("jobs")) { nb_jobs_per_task = cli["jobs"].as<int>(); }
                if (cli.count("totalu")) { total_utilization = cli["totalu"].as<double>(); }
                if (cli.count("output")) { output_filepath = cli["output"].as<std::string>(); }
        }
        catch (cxxopts::exceptions::parsing& e) {
                std::cerr << "Error parsing command-line options: " << e.what() << std::endl;
                return EXIT_FAILURE;
        }
        catch (std::bad_cast& e) {
                std::cerr << "Error parsing casting option: " << e.what() << std::endl;
                return EXIT_FAILURE;
        }

        // Generate task set
        scenario::setting taskset{};

        if (nb_cores <= 0) {
                std::cerr << "There must be at least one core to execute the taskset" << std::endl;
                return EXIT_FAILURE;
        }
        taskset.nb_cores = nb_cores;

        try {
                taskset.tasks = generate_taskset(nb_tasks, MAX_PERIOD, total_utilization);
        }
        catch (std::invalid_argument& e) {
                std::cerr << "Invalide argument: " << e.what() << std::endl;
                return EXIT_FAILURE;
        }

        // Generate jobs and arrivals for each task
        for (auto& gen_task : taskset.tasks) {
                generate_jobs(gen_task, nb_jobs_per_task);
        }

        // Write the scenario to output file
        scenario::write_file(output_filepath, taskset);

        return EXIT_SUCCESS;
}
