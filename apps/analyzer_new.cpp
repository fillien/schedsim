#include <schedsim/io/error.hpp>
#include <schedsim/io/metrics.hpp>

#include <cxxopts.hpp>

#include <iomanip>
#include <iostream>
#include <string>

namespace {

namespace io = schedsim::io;

struct Config {
    std::string trace_file;
    bool summary{true};
    bool deadline_misses{false};
    bool response_times{false};
    bool energy{false};
    std::string format{"text"};
};

Config parse_args(int argc, char** argv) {
    cxxopts::Options options("schedview-new", "Trace analyzer");

    options.add_options()
        ("trace-file", "JSON trace file", cxxopts::value<std::string>())
        ("summary", "Print summary (default)")
        ("deadline-misses", "Show deadline miss details")
        ("response-times", "Show response time stats")
        ("energy", "Show energy breakdown")
        ("format", "Format: text|csv|json (default: text)", cxxopts::value<std::string>()->default_value("text"))
        ("h,help", "Show help");

    options.parse_positional({"trace-file"});
    options.positional_help("<trace-file>");

    auto result = options.parse(argc, argv);

    if (result.count("help") != 0U) {
        std::cout << options.help() << std::endl;
        std::exit(0);
    }

    if (result.count("trace-file") == 0U) {
        std::cerr << "Error: trace file is required" << std::endl;
        std::cerr << options.help() << std::endl;
        std::exit(64);
    }

    Config config;
    config.trace_file = result["trace-file"].as<std::string>();
    config.summary = true;  // Always show summary
    config.deadline_misses = result.count("deadline-misses") != 0U;
    config.response_times = result.count("response-times") != 0U;
    config.energy = result.count("energy") != 0U;
    config.format = result["format"].as<std::string>();

    return config;
}

void print_text_output(const Config& config, const io::SimulationMetrics& metrics) {
    std::cout << "=== Simulation Metrics ===" << std::endl;
    std::cout << std::endl;

    // Basic stats
    std::cout << "Jobs:" << std::endl;
    std::cout << "  Total:           " << metrics.total_jobs << std::endl;
    std::cout << "  Completed:       " << metrics.completed_jobs << std::endl;
    std::cout << "  Deadline misses: " << metrics.deadline_misses << std::endl;
    std::cout << std::endl;

    std::cout << "Scheduling:" << std::endl;
    std::cout << "  Preemptions:      " << metrics.preemptions << std::endl;
    std::cout << "  Context switches: " << metrics.context_switches << std::endl;
    std::cout << std::endl;

    // Utilization
    if (!metrics.utilization_per_processor.empty()) {
        std::cout << "Utilization:" << std::endl;
        std::cout << "  Average: " << std::fixed << std::setprecision(2)
                  << (metrics.average_utilization * 100.0) << "%" << std::endl;
        for (const auto& [proc, util] : metrics.utilization_per_processor) {
            std::cout << "  Processor " << proc << ": " << std::fixed << std::setprecision(2)
                      << (util * 100.0) << "%" << std::endl;
        }
        std::cout << std::endl;
    }

    // Energy
    if (config.energy && metrics.total_energy_mj > 0) {
        std::cout << "Energy:" << std::endl;
        std::cout << "  Total: " << std::fixed << std::setprecision(3)
                  << metrics.total_energy_mj << " mJ" << std::endl;
        for (const auto& [proc, energy] : metrics.energy_per_processor) {
            std::cout << "  Processor " << proc << ": " << std::fixed << std::setprecision(3)
                      << energy << " mJ" << std::endl;
        }
        std::cout << std::endl;
    }

    // Response times
    if (config.response_times && !metrics.response_times_per_task.empty()) {
        std::cout << "Response Times (per task):" << std::endl;
        for (const auto& [tid, times] : metrics.response_times_per_task) {
            if (times.empty()) {
                continue;
            }
            auto stats = io::compute_response_time_stats(times);
            std::cout << "  Task " << tid << ":" << std::endl;
            std::cout << "    Min:    " << std::fixed << std::setprecision(6) << stats.min << " s" << std::endl;
            std::cout << "    Max:    " << std::fixed << std::setprecision(6) << stats.max << " s" << std::endl;
            std::cout << "    Mean:   " << std::fixed << std::setprecision(6) << stats.mean << " s" << std::endl;
            std::cout << "    Median: " << std::fixed << std::setprecision(6) << stats.median << " s" << std::endl;
            std::cout << "    Stddev: " << std::fixed << std::setprecision(6) << stats.stddev << " s" << std::endl;
            std::cout << "    P95:    " << std::fixed << std::setprecision(6) << stats.percentile_95 << " s" << std::endl;
            std::cout << "    P99:    " << std::fixed << std::setprecision(6) << stats.percentile_99 << " s" << std::endl;
        }
    }
}

void print_csv_output(const Config& config, const io::SimulationMetrics& metrics) {
    // Header
    std::cout << "metric,value" << std::endl;

    // Basic stats
    std::cout << "total_jobs," << metrics.total_jobs << std::endl;
    std::cout << "completed_jobs," << metrics.completed_jobs << std::endl;
    std::cout << "deadline_misses," << metrics.deadline_misses << std::endl;
    std::cout << "preemptions," << metrics.preemptions << std::endl;
    std::cout << "context_switches," << metrics.context_switches << std::endl;
    std::cout << "average_utilization," << metrics.average_utilization << std::endl;

    if (config.energy) {
        std::cout << "total_energy_mj," << metrics.total_energy_mj << std::endl;
    }

    // Per-processor utilization
    for (const auto& [proc, util] : metrics.utilization_per_processor) {
        std::cout << "utilization_proc_" << proc << "," << util << std::endl;
    }

    // Per-processor energy
    if (config.energy) {
        for (const auto& [proc, energy] : metrics.energy_per_processor) {
            std::cout << "energy_proc_" << proc << "_mj," << energy << std::endl;
        }
    }

    // Response time stats
    if (config.response_times) {
        for (const auto& [tid, times] : metrics.response_times_per_task) {
            if (times.empty()) {
                continue;
            }
            auto stats = io::compute_response_time_stats(times);
            std::cout << "response_time_task_" << tid << "_min," << stats.min << std::endl;
            std::cout << "response_time_task_" << tid << "_max," << stats.max << std::endl;
            std::cout << "response_time_task_" << tid << "_mean," << stats.mean << std::endl;
            std::cout << "response_time_task_" << tid << "_p95," << stats.percentile_95 << std::endl;
            std::cout << "response_time_task_" << tid << "_p99," << stats.percentile_99 << std::endl;
        }
    }
}

void print_json_output(const Config& config, const io::SimulationMetrics& metrics) {
    std::cout << "{" << std::endl;
    std::cout << "  \"total_jobs\": " << metrics.total_jobs << "," << std::endl;
    std::cout << "  \"completed_jobs\": " << metrics.completed_jobs << "," << std::endl;
    std::cout << "  \"deadline_misses\": " << metrics.deadline_misses << "," << std::endl;
    std::cout << "  \"preemptions\": " << metrics.preemptions << "," << std::endl;
    std::cout << "  \"context_switches\": " << metrics.context_switches << "," << std::endl;
    std::cout << "  \"average_utilization\": " << metrics.average_utilization;

    if (config.energy && metrics.total_energy_mj > 0) {
        std::cout << "," << std::endl;
        std::cout << "  \"total_energy_mj\": " << metrics.total_energy_mj;
    }

    if (config.response_times && !metrics.response_times_per_task.empty()) {
        std::cout << "," << std::endl;
        std::cout << "  \"response_times\": {" << std::endl;
        bool first = true;
        for (const auto& [tid, times] : metrics.response_times_per_task) {
            if (times.empty()) {
                continue;
            }
            if (!first) {
                std::cout << "," << std::endl;
            }
            first = false;
            auto stats = io::compute_response_time_stats(times);
            std::cout << "    \"task_" << tid << "\": {" << std::endl;
            std::cout << "      \"min\": " << stats.min << "," << std::endl;
            std::cout << "      \"max\": " << stats.max << "," << std::endl;
            std::cout << "      \"mean\": " << stats.mean << "," << std::endl;
            std::cout << "      \"median\": " << stats.median << "," << std::endl;
            std::cout << "      \"stddev\": " << stats.stddev << "," << std::endl;
            std::cout << "      \"p95\": " << stats.percentile_95 << "," << std::endl;
            std::cout << "      \"p99\": " << stats.percentile_99 << std::endl;
            std::cout << "    }";
        }
        std::cout << std::endl;
        std::cout << "  }";
    }

    std::cout << std::endl;
    std::cout << "}" << std::endl;
}

} // anonymous namespace

int main(int argc, char** argv) {
    try {
        auto config = parse_args(argc, argv);

        auto metrics = io::compute_metrics_from_file(config.trace_file);

        if (config.format == "text") {
            print_text_output(config, metrics);
        } else if (config.format == "csv") {
            print_csv_output(config, metrics);
        } else if (config.format == "json") {
            print_json_output(config, metrics);
        } else {
            std::cerr << "Unknown format: " << config.format << std::endl;
            return 64;
        }

        return 0;
    }
    catch (const io::LoaderError& e) {
        std::cerr << "Error loading trace: " << e.what() << std::endl;
        return 1;
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
