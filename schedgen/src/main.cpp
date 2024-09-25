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

#include <nlohmann/json.hpp>

#include <fstream>

namespace fs = std::filesystem;

struct taskset_config {
        fs::path output_filepath{"scenario.json"};
        std::size_t nb_tasks{0};
        double total_utilization{1};
        double success_rate{1};
};

struct convert_config {
        fs::path file_to_convert{"taskset.txt"};
        fs::path output_filepath{"scenario.json"};
};

struct platform_config {
        fs::path output_filepath{"platform.json"};
        std::size_t nb_procs{0};
        std::vector<double> frequencies{0};
        double effective_freq{0};
};

auto parse_args_taskset(const int argc, const char** argv) -> taskset_config
{
        taskset_config config;

        // clang-format off
        cxxopts::Options options("schedgen taskset", "Generate task for monocore and multicore systems");
        options.add_options()
                ("h,help", "Helper")
                ("t,tasks", "Number of tasks to generate", cxxopts::value<int>())
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
        config.total_utilization = cli["totalu"].as<double>();
        config.success_rate = cli["success"].as<double>();
        if (cli.count("output")) { config.output_filepath = cli["output"].as<std::string>(); }

        return config;
}

auto parse_args_convert(const int argc, const char** argv) -> convert_config
{
        convert_config config;

        // clang-format off
        cxxopts::Options options("schedgen taskset", "Generate task for monocore and multicore systems");
        options.add_options()
                ("h,help", "Helper")
                ("i,input", "The taskset to convert", cxxopts::value<std::string>())
                ("o,output", "Output file to write the scenario", cxxopts::value<std::string>());
        // clang-format on
        const auto cli = options.parse(argc, argv);

        if (cli.count("help") || cli.arguments().empty()) {
                std::cout << options.help() << std::endl;
                exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        if (cli.count("input")) { config.file_to_convert = cli["input"].as<std::string>(); }
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
		("e,eff", "Add a effective frequency", cxxopts::value<double>())
		("o,output", "Output file to write the scenario", cxxopts::value<std::string>());
        // clang-format on

        const auto cli = options.parse(argc, argv);

        if (cli.count("help") || cli.arguments().empty()) {
                std::cout << options.help() << std::endl;
                exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        config.nb_procs = cli["cores"].as<std::size_t>();
        config.frequencies = cli["freq"].as<std::vector<double>>();
        config.effective_freq = cli["eff"].as<double>();
        if (cli.count("output")) { config.output_filepath = cli["output"].as<std::string>(); }
        return config;
}

std::vector<std::string> splitByLine(const std::string& str)
{
        std::vector<std::string> lines;
        std::istringstream stream(str);
        std::string line;

        // Use std::getline to split by newline characters
        while (std::getline(stream, line)) {
                lines.push_back(line);
        }

        return lines;
}

std::vector<std::string> splitBySpace(const std::string& str)
{
        std::vector<std::string> words;
        std::istringstream stream(str);
        std::string word;

        // Use the >> operator to split by spaces
        while (stream >> word) {
                words.push_back(word);
        }

        return words;
}

auto gen_jobs(protocols::scenario::task& new_task, const double& duration) -> void
{
        const double WALL_TIME{5000};
        double time{0};

        while (time <= WALL_TIME) {
                new_task.jobs.push_back({.arrival = time, .duration = duration});
                time += new_task.period;
        }
}

auto convert(const convert_config& config) -> protocols::scenario::setting
{
        using namespace protocols::scenario;

        std::ifstream input_file(config.file_to_convert);
        if (!input_file) {
                throw std::runtime_error("Failed to open file: " + config.file_to_convert.string());
        }

        const std::string input(
            (std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
        input_file.close();

        const auto lines = splitByLine(input);

        protocols::scenario::setting sce;

        std::size_t cpt_id{1};

        for (const auto& line : lines) {
                const auto words{splitBySpace(line)};
                double utilization{std::stod(words.at(1))};
                double wcet{std::stod(words.at(2))};
                double period{std::stod(words.at(3))};
                task t{
                    .id = cpt_id,
                    .utilization = utilization,
                    .period = period,
                    .jobs = std::vector<job>{}};
                gen_jobs(t, wcet);
                sce.tasks.push_back(t);
                cpt_id++;
        }

        return sce;
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
                            config.nb_tasks, config.total_utilization, config.success_rate);
                        // Write the scenario to output file
                        protocols::scenario::write_file(config.output_filepath, taskset);
                }
                else if (command == "platform") {
                        const auto config = parse_args_platform(argc - 1, argv + 1);
                        protocols::hardware::write_file(
                            config.output_filepath,
                            {config.nb_procs, config.frequencies, config.effective_freq});
                }
                else if (command == "convert") {
                        const auto config{parse_args_convert(argc - 1, argv + 1)};
                        const auto taskset{convert(config)};
                        protocols::scenario::write_file(config.output_filepath, taskset);
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
