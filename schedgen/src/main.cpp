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
        double total_utilization{1};
        double umax{1};
        double success_rate{1};
        double compression_rate{1};
};

struct platform_config {
        fs::path output_filepath{"platform.json"};
        std::size_t nb_procs{0};
        std::vector<double> frequencies{0};
        double effective_freq{0};
        std::vector<double> power_model{0};
};

auto parse_args_taskset(const int argc, const char** argv) -> taskset_config
{
        taskset_config config;

        // clang-format off
        cxxopts::Options options("schedgen taskset", "Task Set Generator for Mono-core and Multi-core Systems");
        options.add_options()
                ("h,help", "Show this help message.")
                ("t,tasks", "Specify the number of tasks to generate.", cxxopts::value<int>())
                ("u,totalu", "Set the total utilization of the task set.", cxxopts::value<double>())
                ("m,umax", "Define the maximum utilization for a task (range: 0 to 1).", cxxopts::value<double>())
                ("s,success", "Specify the success rate of deadlines met (range: 0 to 1).", cxxopts::value<double>())
                ("c,compression", "Set the compression ratio for the tasks (range: 0 to 1).", cxxopts::value<double>())
                ("o,output", "Output file to write the generated scenario.", cxxopts::value<std::string>());
        // clang-format on
        const auto cli = options.parse(argc, argv);

        if (cli.count("help") || cli.arguments().empty()) {
                std::cout << options.help() << std::endl;
                exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        config.nb_tasks = cli["tasks"].as<int>();
        config.total_utilization = cli["totalu"].as<double>();
        config.success_rate = cli["success"].as<double>();
        config.umax = cli["umax"].as<double>();
        config.compression_rate = cli["compression"].as<double>();
        if (cli.count("output")) { config.output_filepath = cli["output"].as<std::string>(); }

        return config;
}

auto parse_args_platform(const int argc, const char** argv) -> platform_config
{
        platform_config config;

        // clang-format off
        cxxopts::Options options("schedgen platform", "Platform Configuration File Generator");
        options.add_options()
                ("h,help", "Show this help message.")
		        ("c,cores", "Specify the number of processor cores.", cxxopts::value<std::size_t>())
		        ("f,freq", "Define the allowed operating frequencies.", cxxopts::value<std::vector<double>>())
		        ("e,eff", "Add an effective frequency (actual frequency that minimize the total energy consumption).", cxxopts::value<double>())
                ("p,power", "Set the power model for the platform.", cxxopts::value<std::vector<double>>())
		        ("o,output", "Specify the output file to write the configuration.", cxxopts::value<std::string>());
        // clang-format on

        const auto cli = options.parse(argc, argv);

        if (cli.count("help") || cli.arguments().empty()) {
                std::cout << options.help() << std::endl;
                exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        config.nb_procs = cli["cores"].as<std::size_t>();
        config.frequencies = cli["freq"].as<std::vector<double>>();
        config.effective_freq = cli["eff"].as<double>();
        config.power_model = cli["power"].as<std::vector<double>>();
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
                        const auto config = parse_args_taskset(argc - 1, argv + 1);
                        const auto taskset = generate_taskset(
                            config.nb_tasks,
                            config.total_utilization,
                            config.umax,
                            config.success_rate,
                            config.compression_rate);
                        // Write the scenario to output file
                        protocols::scenario::write_file(config.output_filepath, taskset);
                }
                else if (command == "platform") {
                        const auto config = parse_args_platform(argc - 1, argv + 1);
                        protocols::hardware::write_file(
                            config.output_filepath,
                            {config.nb_procs,
                             config.frequencies,
                             config.effective_freq,
                             config.power_model});
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
