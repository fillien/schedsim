#include <schedsim/core/types.hpp>

#include <schedsim/io/scenario_generation.hpp>
#include <schedsim/io/scenario_loader.hpp>

#include <cxxopts.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <thread>

namespace {

namespace core = schedsim::core;
namespace io = schedsim::io;

enum class PeriodMode { Range, Harmonic };

struct Config {
    std::size_t num_tasks{0};
    double target_utilization{0.0};
    double umin{0.0};
    double umax{1.0};
    double period_min_ms{10.0};
    double period_max_ms{1000.0};
    bool log_uniform{true};
    PeriodMode period_mode{PeriodMode::Range};
    double success_rate{1.0};
    double compression_rate{1.0};
    double duration{0.0};  // 0 = use hyperperiod for harmonic, no jobs for range
    double exec_ratio{1.0};
    std::string output_file{"-"};
    std::optional<uint64_t> seed;
    std::size_t batch_count{0};
    std::string batch_dir;
    std::size_t num_threads{0};  // 0 = hardware concurrency
};

Config parse_args(int argc, char** argv) {
    cxxopts::Options options("schedgen-new", "Task set generator (UUniFast-Discard-Weibull)");

    // clang-format off
    options.add_options()
        ("n,tasks", "Number of tasks (required)", cxxopts::value<std::size_t>())
        ("t", "Alias for --tasks", cxxopts::value<std::size_t>())
        ("u,utilization", "Target total utilization (required, can exceed 1.0 for multicore)",
            cxxopts::value<double>())
        ("totalu", "Alias for --utilization", cxxopts::value<double>())
        ("umin", "Min per-task utilization [0,1] (default: 0)", cxxopts::value<double>()->default_value("0"))
        ("umax", "Max per-task utilization [0,1] (default: 1)", cxxopts::value<double>()->default_value("1"))
        ("s,success", "Success rate for deadline budget [0,1] (default: 1)", cxxopts::value<double>()->default_value("1"))
        ("c,compression", "Compression ratio (min duration/WCET) [0,1] (default: 1)",
            cxxopts::value<double>()->default_value("1"))
        ("period-min", "Min period in ms (default: 10, range mode only)", cxxopts::value<double>()->default_value("10"))
        ("period-max", "Max period in ms (default: 1000, range mode only)", cxxopts::value<double>()->default_value("1000"))
        ("log-uniform", "Log-uniform periods (default for range mode)")
        ("uniform", "Uniform periods (range mode only)")
        ("period-mode", "Period selection: 'harmonic' (fixed set) or 'range' (default: range)",
            cxxopts::value<std::string>()->default_value("range"))
        ("d,duration", "Simulation duration in seconds (range mode only, 0 = no jobs)",
            cxxopts::value<double>()->default_value("0"))
        ("exec-ratio", "Actual/WCET ratio (default: 1.0, range mode only)", cxxopts::value<double>()->default_value("1.0"))
        ("o,output", "Output file (default: stdout)", cxxopts::value<std::string>()->default_value("-"))
        ("seed", "Random seed", cxxopts::value<uint64_t>())
        ("batch", "Generate multiple scenarios", cxxopts::value<std::size_t>())
        ("T,tasksets", "Alias for --batch", cxxopts::value<std::size_t>())
        ("dir", "Output directory for batch", cxxopts::value<std::string>())
        ("threads", "Parallel threads for batch (default: hardware concurrency)",
            cxxopts::value<std::size_t>()->default_value("0"))
        ("h,help", "Show help");
    // clang-format on

    auto result = options.parse(argc, argv);

    if (result.count("help") != 0U) {
        std::cout << options.help() << std::endl;
        std::exit(0);
    }

    Config config;

    // Handle task count aliases
    if (result.count("tasks") != 0U) {
        config.num_tasks = result["tasks"].as<std::size_t>();
    } else if (result.count("t") != 0U) {
        config.num_tasks = result["t"].as<std::size_t>();
    } else {
        std::cerr << "Error: --tasks (-n or -t) is required" << std::endl;
        std::exit(64);
    }

    // Handle utilization aliases
    if (result.count("utilization") != 0U) {
        config.target_utilization = result["utilization"].as<double>();
    } else if (result.count("totalu") != 0U) {
        config.target_utilization = result["totalu"].as<double>();
    } else {
        std::cerr << "Error: --utilization (-u or --totalu) is required" << std::endl;
        std::exit(64);
    }

    config.umin = result["umin"].as<double>();
    config.umax = result["umax"].as<double>();
    config.success_rate = result["success"].as<double>();
    config.compression_rate = result["compression"].as<double>();
    config.period_min_ms = result["period-min"].as<double>();
    config.period_max_ms = result["period-max"].as<double>();
    config.log_uniform = result.count("uniform") == 0U;
    config.duration = result["duration"].as<double>();
    config.exec_ratio = result["exec-ratio"].as<double>();
    config.output_file = result["output"].as<std::string>();
    config.num_threads = result["threads"].as<std::size_t>();

    // Period mode
    std::string mode_str = result["period-mode"].as<std::string>();
    if (mode_str == "harmonic") {
        config.period_mode = PeriodMode::Harmonic;
    } else if (mode_str == "range") {
        config.period_mode = PeriodMode::Range;
    } else {
        std::cerr << "Error: --period-mode must be 'harmonic' or 'range'" << std::endl;
        std::exit(64);
    }

    if (result.count("seed") != 0U) {
        config.seed = result["seed"].as<uint64_t>();
    }

    // Handle batch aliases
    if (result.count("batch") != 0U) {
        config.batch_count = result["batch"].as<std::size_t>();
    } else if (result.count("tasksets") != 0U) {
        config.batch_count = result["tasksets"].as<std::size_t>();
    }

    if (config.batch_count > 0) {
        if (result.count("dir") != 0U) {
            config.batch_dir = result["dir"].as<std::string>();
        } else {
            std::cerr << "Error: --dir is required with --batch or -T" << std::endl;
            std::exit(64);
        }
    }

    // Validation
    if (config.num_tasks < 1) {
        std::cerr << "Error: num_tasks must be >= 1" << std::endl;
        std::exit(64);
    }

    if (config.target_utilization <= 0.0) {
        std::cerr << "Error: utilization must be > 0" << std::endl;
        std::exit(64);
    }

    if (config.umin < 0.0 || config.umin > 1.0) {
        std::cerr << "Error: umin must be in [0, 1]" << std::endl;
        std::exit(64);
    }

    if (config.umax < 0.0 || config.umax > 1.0) {
        std::cerr << "Error: umax must be in [0, 1]" << std::endl;
        std::exit(64);
    }

    if (config.umin > config.umax) {
        std::cerr << "Error: umin must be <= umax" << std::endl;
        std::exit(64);
    }

    // Feasibility check for UUniFast-Discard
    double min_possible = static_cast<double>(config.num_tasks) * config.umin;
    double max_possible = static_cast<double>(config.num_tasks) * config.umax;
    if (config.target_utilization < min_possible) {
        std::cerr << "Error: target utilization (" << config.target_utilization
                  << ") < num_tasks * umin (" << min_possible << ")" << std::endl;
        std::exit(64);
    }
    if (config.target_utilization > max_possible) {
        std::cerr << "Error: target utilization (" << config.target_utilization
                  << ") > num_tasks * umax (" << max_possible << ")" << std::endl;
        std::exit(64);
    }

    if (config.success_rate < 0.0 || config.success_rate > 1.0) {
        std::cerr << "Error: success rate must be in [0, 1]" << std::endl;
        std::exit(64);
    }

    if (config.compression_rate < 0.0 || config.compression_rate > 1.0) {
        std::cerr << "Error: compression rate must be in [0, 1]" << std::endl;
        std::exit(64);
    }

    // Range mode specific validation
    if (config.period_mode == PeriodMode::Range) {
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

        // In range mode, per-task utilization must be <= 1
        if (config.target_utilization > static_cast<double>(config.num_tasks)) {
            std::cerr << "Error: in range mode, utilization must be <= num_tasks" << std::endl;
            std::exit(64);
        }
    }

    return config;
}

io::ScenarioData generate_single_harmonic(const Config& config, std::mt19937& rng) {
    io::WeibullJobConfig weibull_config{
        .success_rate = config.success_rate,
        .compression_rate = config.compression_rate
    };

    return io::generate_uunifast_discard_weibull(
        config.num_tasks,
        config.target_utilization,
        config.umin,
        config.umax,
        weibull_config,
        rng
    );
}

io::ScenarioData generate_single_range(const Config& config, std::mt19937& rng) {
    io::PeriodDistribution period_dist{
        .min = core::duration_from_seconds(config.period_min_ms / 1000.0),
        .max = core::duration_from_seconds(config.period_max_ms / 1000.0),
        .log_uniform = config.log_uniform
    };

    io::ScenarioData scenario;
    if (config.duration > 0) {
        scenario = io::generate_scenario(
            config.num_tasks,
            config.target_utilization,
            period_dist,
            core::duration_from_seconds(config.duration),
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

    // Update task IDs to start at 1
    for (std::size_t idx = 0; idx < scenario.tasks.size(); ++idx) {
        scenario.tasks[idx].id = idx + 1;
    }

    return scenario;
}

io::ScenarioData generate_single(const Config& config, std::mt19937& rng) {
    if (config.period_mode == PeriodMode::Harmonic) {
        return generate_single_harmonic(config, rng);
    } else {
        return generate_single_range(config, rng);
    }
}

void generate_batch_parallel(const Config& config, std::mt19937& base_rng) {
    std::filesystem::create_directories(config.batch_dir);

    std::size_t num_threads = config.num_threads;
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) {
            num_threads = 1;
        }
    }

    // Queue of indices to process
    std::queue<std::size_t> task_queue;
    for (std::size_t idx = 1; idx <= config.batch_count; ++idx) {
        task_queue.push(idx);
    }

    std::mutex queue_mutex;
    std::atomic<std::size_t> completed{0};

    // Create worker threads
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (std::size_t thread_idx = 0; thread_idx < num_threads; ++thread_idx) {
        // Create per-thread RNG with unique seed
        uint64_t thread_seed = base_rng();

        threads.emplace_back([&config, &task_queue, &queue_mutex, &completed, thread_seed]() {
            std::mt19937 rng{static_cast<std::mt19937::result_type>(thread_seed)};

            while (true) {
                std::size_t index;
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    if (task_queue.empty()) {
                        break;
                    }
                    index = task_queue.front();
                    task_queue.pop();
                }

                auto scenario = generate_single(config, rng);

                std::filesystem::path filepath =
                    std::filesystem::path(config.batch_dir) / (std::to_string(index) + ".json");

                std::ofstream outfile(filepath);
                if (!outfile) {
                    std::cerr << "Error: cannot open file: " << filepath << std::endl;
                    continue;
                }

                io::write_scenario_to_stream(scenario, outfile);
                ++completed;
            }
        });
    }

    // Join all threads
    for (auto& thr : threads) {
        thr.join();
    }

    std::cerr << "Generated " << completed.load() << " scenarios in " << config.batch_dir
              << " using " << num_threads << " threads" << std::endl;
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
            // Batch mode with parallel execution
            generate_batch_parallel(config, rng);
        } else {
            // Single scenario mode
            auto scenario = generate_single(config, rng);

            if (config.output_file == "-") {
                io::write_scenario_to_stream(scenario, std::cout);
            } else {
                std::ofstream outfile(config.output_file);
                if (!outfile) {
                    std::cerr << "Error: cannot open file: " << config.output_file << std::endl;
                    return 1;
                }
                io::write_scenario_to_stream(scenario, outfile);
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
