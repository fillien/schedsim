#include <format>
#include <generators/uunifast_discard_weibull.hpp>
#include <protocols/hardware.hpp>
#include <protocols/scenario.hpp>

#include <cstddef>
#include <cstdlib>
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

namespace fs = std::filesystem;

struct TasksetConfig {
        fs::path output_filepath{"scenario.json"};
        fs::path output_dir{"scenarios"};
        int nb_tasksets{0};
        int nb_tasks{0};
        double total_utilization{1};
        double umax{1};
        double umin{0};
        double success_rate{1};
        double compression_rate{1};
};

auto parse_args_taskset(const int argc, const char** argv) -> TasksetConfig
{
        TasksetConfig config;

        // clang-format off
        cxxopts::Options options("schedgen taskset", "Task Set Generator for Mono-core and Multi-core Systems");
        options.add_options()
                ("h,help", "Show this help message.")
                ("dir", "Output directory of the sceanarios", cxxopts::value<std::string>())
                ("T, tasksets", "The number of tasksets to generate", cxxopts::value<int>())
                ("t,tasks", "Specify the number of tasks to generate.", cxxopts::value<int>())
                ("u,totalu", "Set the total utilization of the task set.", cxxopts::value<double>())
                ("m,umax", "Define the maximum utilization for a task (range: 0 to 1).", cxxopts::value<double>())
                ("n,umin", "Define the minimum utilization for a task (range: 0 to 1).", cxxopts::value<double>())
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
        config.umin = cli["umin"].as<double>();
        config.compression_rate = cli["compression"].as<double>();

        if (cli.count("output") && cli.count("dir")) {
                throw std::invalid_argument(
                    "can't have output file and output directory specified at the same time.");
        }

        if (cli.count("output")) { config.output_filepath = cli["output"].as<std::string>(); }
        else if (cli.count("dir")) {
                config.output_dir = cli["dir"].as<std::string>();
                config.nb_tasksets = cli["tasksets"].as<int>();
        }

        return config;
}

auto main(const int argc, const char** argv) -> int
{
        try {
                const auto config = parse_args_taskset(argc, argv);
                if (config.nb_tasksets > 1) {
                        generators::generate_tasksets(
                            config.output_dir,
                            config.nb_tasksets,
                            config.nb_tasks,
                            config.total_utilization,
                            config.umax,
                            config.umin,
                            config.success_rate,
                            config.compression_rate);
                }
                else {
                        const auto taskset = generators::uunifast_discard_weibull(
                            config.nb_tasks,
                            config.total_utilization,
                            config.umax,
                            config.umin,
                            config.success_rate,
                            config.compression_rate);
                        protocols::scenario::write_file(config.output_filepath, taskset);
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
