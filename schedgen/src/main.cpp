#include "scenario.hpp"
#include "task_generator.hpp"

#include <cstddef>
#include <cstdlib>
#include <cxxopts.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <typeinfo>

namespace fs = std::filesystem;

struct app_config {
        fs::path output_filepath{"scenario.json"};
        std::size_t nb_cores{0};
        std::size_t nb_tasks{0};
        std::size_t nb_jobs{0};
        double total_utilization{1};
        double success_rate{1};
};

auto parse_args(int argc, char** argv) -> app_config
{
        app_config config;

        // clang-format off
        cxxopts::Options options("generator", "Generate task for monocore and multicore systems");
        options.add_options()
                ("h,help", "Helper")
                ("c,cores", "Number of cores", cxxopts::value<int>())
                ("t,tasks", "Number of tasks to generate", cxxopts::value<int>())
                ("j,jobs", "Number of jobs", cxxopts::value<int>())
                ("u,totalu", "Total utilization of the taskset", cxxopts::value<double>())
                ("s,success", "Rate of deadlines met (0..1)", cxxopts::value<double>())
                ("o,output", "Output file to write the scenario", cxxopts::value<std::string>());
        // clang-format on
        const auto cli = options.parse(argc, argv);

        if (cli.count("help") || cli.arguments().empty()) {
                std::cout << options.help() << std::endl;
                exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        config.nb_cores = cli["cores"].as<int>();
        config.nb_tasks = cli["tasks"].as<int>();
        config.nb_jobs = cli["jobs"].as<int>();
        config.total_utilization = cli["totalu"].as<double>();
        config.success_rate = cli["success"].as<double>();
        if (cli.count("output")) { config.output_filepath = cli["output"].as<std::string>(); }

        return config;
}

auto main(int argc, char** argv) -> int
{
        try {
                auto config = parse_args(argc, argv);

                if (config.nb_cores <= 0) {
                        std::cerr << "There must be at least one core to execute the taskset"
                                  << std::endl;
                        return EXIT_FAILURE;
                }

                auto taskset = generate_taskset(
                    config.nb_cores,
                    config.nb_tasks,
                    config.nb_jobs,
                    config.total_utilization,
                    config.success_rate);
                // Write the scenario to output file
                scenario::write_file(config.output_filepath, taskset);

                return EXIT_SUCCESS;
        }
        catch (const cxxopts::exceptions::parsing& e) {
                std::cerr << "Error parsing options: " << e.what() << std::endl;
        }
        catch (std::bad_cast& e) {
                std::cerr << "Error parsing casting option: " << e.what() << std::endl;
        }
        catch (const std::invalid_argument& e) {
                std::cerr << "Invalid argument: " << e.what() << std::endl;
        }
        catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
        }
        return EXIT_FAILURE;
}
