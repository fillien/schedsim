#include <schedsim/core/types.hpp>

#include <schedsim/io/scenario_generation.hpp>
#include <schedsim/io/scenario_loader.hpp>

#include <cxxopts.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <string>

namespace {

namespace core = schedsim::core;
namespace io = schedsim::io;

struct Config {
    std::size_t num_tasks{0};
    double target_utilization{0.0};
    double period_min_ms{10.0};
    double period_max_ms{1000.0};
    bool log_uniform{true};
    double duration{0.0};  // 0 = no jobs
    double exec_ratio{1.0};
    std::string output_file{"-"};
    std::optional<uint64_t> seed;
    std::size_t batch_count{0};
    std::string batch_dir;
};

Config parse_args(int argc, char** argv) {
    cxxopts::Options options("schedgen-new", "Task set generator");

    options.add_options()
        ("n,tasks", "Number of tasks (required)", cxxopts::value<std::size_t>())
        ("u,utilization", "Target utilization (required)", cxxopts::value<double>())
        ("period-min", "Min period in ms (default: 10)", cxxopts::value<double>()->default_value("10"))
        ("period-max", "Max period in ms (default: 1000)", cxxopts::value<double>()->default_value("1000"))
        ("log-uniform", "Log-uniform periods (default)")
        ("uniform", "Uniform periods")
        ("d,duration", "Simulation duration for jobs in seconds", cxxopts::value<double>()->default_value("0"))
        ("exec-ratio", "Actual/WCET ratio (default: 1.0)", cxxopts::value<double>()->default_value("1.0"))
        ("o,output", "Output file (default: stdout)", cxxopts::value<std::string>()->default_value("-"))
        ("seed", "Random seed", cxxopts::value<uint64_t>())
        ("batch", "Generate multiple scenarios", cxxopts::value<std::size_t>())
        ("dir", "Output directory for batch", cxxopts::value<std::string>())
        ("h,help", "Show help");

    auto result = options.parse(argc, argv);

    if (result.count("help") != 0U) {
        std::cout << options.help() << std::endl;
        std::exit(0);
    }

    if (result.count("tasks") == 0U) {
        std::cerr << "Error: --tasks is required" << std::endl;
        std::exit(64);
    }

    if (result.count("utilization") == 0U) {
        std::cerr << "Error: --utilization is required" << std::endl;
        std::exit(64);
    }

    Config config;
    config.num_tasks = result["tasks"].as<std::size_t>();
    config.target_utilization = result["utilization"].as<double>();
    config.period_min_ms = result["period-min"].as<double>();
    config.period_max_ms = result["period-max"].as<double>();
    config.log_uniform = result.count("uniform") == 0U;
    config.duration = result["duration"].as<double>();
    config.exec_ratio = result["exec-ratio"].as<double>();
    config.output_file = result["output"].as<std::string>();

    if (result.count("seed") != 0U) {
        config.seed = result["seed"].as<uint64_t>();
    }

    if (result.count("batch") != 0U) {
        config.batch_count = result["batch"].as<std::size_t>();
        if (result.count("dir") != 0U) {
            config.batch_dir = result["dir"].as<std::string>();
        } else {
            std::cerr << "Error: --dir is required with --batch" << std::endl;
            std::exit(64);
        }
    }

    // Validation
    if (config.target_utilization <= 0.0 || config.target_utilization > 1.0) {
        std::cerr << "Error: utilization must be in (0, 1]" << std::endl;
        std::exit(64);
    }

    if (config.period_min_ms <= 0.0 || config.period_max_ms <= 0.0) {
        std::cerr << "Error: periods must be positive" << std::endl;
        std::exit(64);
    }

    if (config.period_min_ms > config.period_max_ms) {
        std::cerr << "Error: period-min must be <= period-max" << std::endl;
        std::exit(64);
    }

    if (config.exec_ratio <= 0.0 || config.exec_ratio > 1.0) {
        std::cerr << "Error: exec-ratio must be in (0, 1]" << std::endl;
        std::exit(64);
    }

    return config;
}

void generate_single(const Config& config, std::mt19937& rng, std::ostream& out) {
    io::PeriodDistribution period_dist{
        .min = core::Duration{config.period_min_ms / 1000.0},
        .max = core::Duration{config.period_max_ms / 1000.0},
        .log_uniform = config.log_uniform
    };

    io::ScenarioData scenario;
    if (config.duration > 0) {
        scenario = io::generate_scenario(
            config.num_tasks,
            config.target_utilization,
            period_dist,
            core::Duration{config.duration},
            rng,
            config.exec_ratio
        );
    } else {
        // Generate tasks without jobs
        scenario.tasks = io::generate_task_set(
            config.num_tasks,
            config.target_utilization,
            period_dist,
            rng
        );
    }

    io::write_scenario_to_stream(scenario, out);
}

} // anonymous namespace

int main(int argc, char** argv) {
    try {
        auto config = parse_args(argc, argv);

        // Initialize RNG
        std::mt19937 rng;
        if (config.seed.has_value()) {
            rng.seed(config.seed.value());
        } else {
            std::random_device rd;
            rng.seed(rd());
        }

        if (config.batch_count > 0) {
            // Batch mode
            std::filesystem::create_directories(config.batch_dir);

            for (std::size_t i = 0; i < config.batch_count; ++i) {
                std::filesystem::path path =
                    std::filesystem::path(config.batch_dir) /
                    ("scenario_" + std::to_string(i) + ".json");

                std::ofstream outfile(path);
                if (!outfile) {
                    std::cerr << "Error: cannot open file: " << path << std::endl;
                    return 1;
                }

                generate_single(config, rng, outfile);
            }

            std::cerr << "Generated " << config.batch_count << " scenarios in "
                      << config.batch_dir << std::endl;
        } else {
            // Single scenario mode
            if (config.output_file == "-") {
                generate_single(config, rng, std::cout);
            } else {
                std::ofstream outfile(config.output_file);
                if (!outfile) {
                    std::cerr << "Error: cannot open file: " << config.output_file << std::endl;
                    return 1;
                }
                generate_single(config, rng, outfile);
            }
        }

        return 0;
    }
    catch (const cxxopts::exceptions::parsing& e) {
        std::cerr << "Invalid args: " << e.what() << std::endl;
        return 64;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
