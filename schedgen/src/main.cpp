#include "task_generator.hpp"
#include <cstddef>
#include <cstdlib>
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <iostream>
#include <protocols/hardware.hpp>
#include <protocols/scenario.hpp>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

namespace fs = std::filesystem;

struct taskset_config {
        fs::path output_filepath{"scenario.json"};
        std::size_t nb_tasks{0};
        std::size_t nb_jobs{0};
        double total_utilization{1};
        double success_rate{1};
};

struct platform_config {
        fs::path output_filepath{"platform.json"};
        std::size_t nb_procs{0};
        std::vector<double> frequencies{0};
};

auto parse_args_taskset(const int argc, const char** argv) -> taskset_config
{
        taskset_config config;

        // clang-format off
        cxxopts::Options options("schedgen taskset", "Generate task for monocore and multicore systems");
        options.add_options()
                ("h,help", "Helper")
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

        config.nb_tasks = cli["tasks"].as<int>();
        config.nb_jobs = cli["jobs"].as<int>();
        config.total_utilization = cli["totalu"].as<double>();
        config.success_rate = cli["success"].as<double>();
        if (cli.count("output")) { config.output_filepath = cli["output"].as<std::string>(); }

        return config;
}

auto parse_args_platform(const int argc, const char** argv) -> platform_config
{
        platform_config config;

        // clang-format off
        cxxopts::Options options("schedgen platform", "Generate platform configuration file");
        options.add_options()
                ("h,help", "Helper")
		("c,cores", "Number of cores", cxxopts::value<std::size_t>())
		("f,freq", "Allowed operating frequencies", cxxopts::value<std::vector<double>>())
		("o,output", "Output file to write the scenario", cxxopts::value<std::string>());
        // clang-format on

        const auto cli = options.parse(argc, argv);

        if (cli.count("help") || cli.arguments().empty()) {
                std::cout << options.help() << std::endl;
                exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        config.nb_procs = cli["cores"].as<std::size_t>();
        config.frequencies = cli["freq"].as<std::vector<double>>();
        if (cli.count("output")) { config.output_filepath = cli["output"].as<std::string>(); }
        return config;
}

auto main(const int argc, const char** argv) -> int
{
        constexpr auto helper{
            "Available commands:\n   taskset   Taskset generator tool\n   platform "
            " Platform generator tool\n\n"};

        try {
                if (argc < 2) {
                        std::cerr << helper;
                        throw std::invalid_argument("No command provided");
                }

                std::string command{argv[1]};
                if (command == "taskset") {
                        auto config = parse_args_taskset(argc - 1, argv + 1);
                        auto taskset = generate_taskset(
                            config.nb_tasks,
                            config.nb_jobs,
                            config.total_utilization,
                            config.success_rate);
                        // Write the scenario to output file
                        protocols::scenario::write_file(config.output_filepath, taskset);
                }
                else if (command == "platform") {
                        auto config = parse_args_platform(argc - 1, argv + 1);
                        protocols::hardware::write_file(
                            config.output_filepath, {config.nb_procs, config.frequencies});
                }
                else {
                        std::cerr << helper << std::endl;
                        throw std::invalid_argument("Unknowed command");
                }

                return EXIT_SUCCESS;
        }
        catch (const cxxopts::exceptions::parsing& e) {
                std::cerr << "Error parsing options: " << e.what() << std::endl;
        }
        catch (const std::bad_cast& e) {
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
