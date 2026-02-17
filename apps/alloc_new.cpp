#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/types.hpp>

#include <schedsim/algo/cluster.hpp>
#include <schedsim/algo/counting_allocator.hpp>
#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/best_fit_allocator.hpp>
#include <schedsim/algo/ff_big_first_allocator.hpp>
#include <schedsim/algo/ff_cap_adaptive_linear_allocator.hpp>
#include <schedsim/algo/ff_cap_adaptive_poly_allocator.hpp>
#include <schedsim/algo/ff_cap_allocator.hpp>
#include <schedsim/algo/ff_lb_allocator.hpp>
#include <schedsim/algo/ff_little_first_allocator.hpp>
#include <schedsim/algo/first_fit_allocator.hpp>
#include <schedsim/algo/mcts_allocator.hpp>
#include <schedsim/algo/multi_cluster_allocator.hpp>
#include <schedsim/algo/worst_fit_allocator.hpp>

#include <schedsim/io/error.hpp>
#include <schedsim/io/metrics.hpp>
#include <schedsim/io/platform_loader.hpp>
#include <schedsim/io/scenario_injection.hpp>
#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/io/trace_writers.hpp>

#include <cxxopts.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

namespace core = schedsim::core;
namespace algo = schedsim::algo;
namespace io = schedsim::io;

struct Config {
    std::string scenario_file;
    std::string platform_file;
    std::string alloc;
    std::unordered_map<std::string, std::string> alloc_args;
    std::optional<double> u_target;
    std::string granularity{"per-cluster"};
    std::string reclaim{"none"};
    std::string dvfs{"none"};
    double dvfs_cooldown_ms{0.0};
    bool verbose{false};
};

Config parse_args(int argc, char** argv) {
    cxxopts::Options options("alloc-new", "Multi-cluster allocator testing tool");

    // clang-format off
    options.add_options()
        ("i,input", "Scenario file (JSON)", cxxopts::value<std::string>())
        ("p,platform", "Platform configuration (JSON)", cxxopts::value<std::string>())
        ("a,alloc", "Allocator: ff_big_first, ff_little_first, ff_cap, "
            "ff_cap_adaptive_linear, ff_cap_adaptive_poly, ff_lb, counting, mcts, "
            "first_fit, worst_fit, best_fit",
            cxxopts::value<std::string>())
        ("g,granularity", "Granularity: per-cluster|per-core (default: per-cluster)",
            cxxopts::value<std::string>()->default_value("per-cluster"))
        ("A,alloc-arg", "Allocator argument key=value (repeatable)",
            cxxopts::value<std::vector<std::string>>())
        ("target", "u_target for LITTLE clusters (default: not set)",
            cxxopts::value<double>())
        ("reclaim", "Reclamation: none|grub|cash (default: none)",
            cxxopts::value<std::string>()->default_value("none"))
        ("dvfs", "DVFS: none|power-aware|ffa|csf|ffa-timer|csf-timer (default: none)",
            cxxopts::value<std::string>()->default_value("none"))
        ("dvfs-cooldown", "DVFS cooldown in ms (default: 0)",
            cxxopts::value<double>()->default_value("0"))
        ("v,verbose", "Verbose stderr output")
        ("h,help", "Show help");
    // clang-format on

    auto result = options.parse(argc, argv);

    if (result.count("help") != 0U) {
        std::cout << options.help() << std::endl;
        std::exit(0);
    }

    if (result.count("input") == 0U) {
        std::cerr << "Error: --input is required" << std::endl;
        std::exit(64);
    }
    if (result.count("platform") == 0U) {
        std::cerr << "Error: --platform is required" << std::endl;
        std::exit(64);
    }
    if (result.count("alloc") == 0U) {
        std::cerr << "Error: --alloc is required" << std::endl;
        std::exit(64);
    }

    Config config;
    config.scenario_file = result["input"].as<std::string>();
    config.platform_file = result["platform"].as<std::string>();
    config.alloc = result["alloc"].as<std::string>();
    config.granularity = result["granularity"].as<std::string>();
    if (config.granularity != "per-cluster" && config.granularity != "per-core") {
        std::cerr << "Error: --granularity must be 'per-cluster' or 'per-core'" << std::endl;
        std::exit(64);
    }
    config.reclaim = result["reclaim"].as<std::string>();
    config.dvfs = result["dvfs"].as<std::string>();
    config.dvfs_cooldown_ms = result["dvfs-cooldown"].as<double>();
    config.verbose = result.count("verbose") != 0U;

    if (result.count("target") != 0U) {
        config.u_target = result["target"].as<double>();
    }

    if (result.count("alloc-arg") != 0U) {
        for (const auto& arg : result["alloc-arg"].as<std::vector<std::string>>()) {
            auto eq = arg.find('=');
            if (eq == std::string::npos) {
                std::cerr << "Error: --alloc-arg must be key=value, got: " << arg << std::endl;
                std::exit(64);
            }
            config.alloc_args[arg.substr(0, eq)] = arg.substr(eq + 1);
        }
    }

    return config;
}

std::unique_ptr<algo::MultiClusterAllocator> make_allocator(
    const Config& config, core::Engine& engine, std::vector<algo::Cluster*>& cluster_ptrs) {
    if (config.alloc == "ff_big_first") {
        return std::make_unique<algo::FFBigFirstAllocator>(engine, cluster_ptrs);
    }
    if (config.alloc == "ff_little_first") {
        return std::make_unique<algo::FFLittleFirstAllocator>(engine, cluster_ptrs);
    }
    if (config.alloc == "ff_cap") {
        return std::make_unique<algo::FFCapAllocator>(engine, cluster_ptrs);
    }
    if (config.alloc == "ff_cap_adaptive_linear") {
        return std::make_unique<algo::FFCapAdaptiveLinearAllocator>(engine, cluster_ptrs);
    }
    if (config.alloc == "ff_cap_adaptive_poly") {
        return std::make_unique<algo::FFCapAdaptivePolyAllocator>(engine, cluster_ptrs);
    }
    if (config.alloc == "ff_lb") {
        return std::make_unique<algo::FFLbAllocator>(engine, cluster_ptrs);
    }
    if (config.alloc == "counting") {
        return std::make_unique<algo::CountingAllocator>(engine, cluster_ptrs);
    }
    if (config.alloc == "first_fit") {
        return std::make_unique<algo::FirstFitAllocator>(engine, cluster_ptrs);
    }
    if (config.alloc == "worst_fit") {
        return std::make_unique<algo::WorstFitAllocator>(engine, cluster_ptrs);
    }
    if (config.alloc == "best_fit") {
        return std::make_unique<algo::BestFitAllocator>(engine, cluster_ptrs);
    }
    if (config.alloc == "mcts") {
        // Parse pattern from --alloc-arg pattern=0,1,0,...
        std::vector<unsigned> pattern;
        auto it = config.alloc_args.find("pattern");
        if (it != config.alloc_args.end()) {
            std::istringstream ss(it->second);
            std::string token;
            while (std::getline(ss, token, ',')) {
                pattern.push_back(static_cast<unsigned>(std::stoul(token)));
            }
        }
        return std::make_unique<algo::MCTSAllocator>(engine, cluster_ptrs, std::move(pattern));
    }

    std::cerr << "Error: unknown allocator: " << config.alloc << std::endl;
    std::exit(64);
}

} // anonymous namespace

int main(int argc, char** argv) {
    try {
        auto config = parse_args(argc, argv);

        if (config.verbose) {
            std::cerr << "Loading platform from: " << config.platform_file << std::endl;
            std::cerr << "Loading scenario from: " << config.scenario_file << std::endl;
        }

        // 1. Create engine and load platform
        core::Engine engine;
        io::load_platform(engine, config.platform_file);

        // 2. Load scenario and inject tasks
        auto scenario = io::load_scenario(config.scenario_file);
        auto scenario_tasks = io::inject_scenario(engine, scenario);

        // 3. Schedule job arrivals
        for (std::size_t i = 0; i < scenario.tasks.size(); ++i) {
            io::schedule_arrivals(engine, *scenario_tasks[i], scenario.tasks[i].jobs);
        }

        // 4. Finalize platform
        engine.platform().finalize();

        // 5. Build clusters (deterministic order)
        auto& platform = engine.platform();
        double ref_freq_max = 0.0;
        for (std::size_t i = 0; i < platform.clock_domain_count(); ++i) {
            ref_freq_max = std::max(ref_freq_max, platform.clock_domain(i).freq_max().mhz);
        }

        std::vector<std::unique_ptr<algo::EdfScheduler>> schedulers;
        std::vector<std::unique_ptr<algo::Cluster>> clusters;
        std::vector<algo::Cluster*> cluster_ptrs;

        if (config.granularity == "per-core") {
            // Per-core: one EdfScheduler + Cluster per processor
            for (std::size_t i = 0; i < platform.processor_count(); ++i) {
                auto& proc = platform.processor(i);
                auto sched = std::make_unique<algo::EdfScheduler>(engine,
                    std::vector<core::Processor*>{&proc});
                double perf = proc.type().performance();
                auto cluster = std::make_unique<algo::Cluster>(
                    proc.clock_domain(), *sched, perf, ref_freq_max);
                cluster->set_processor_id(proc.id());
                cluster_ptrs.push_back(cluster.get());
                schedulers.push_back(std::move(sched));
                clusters.push_back(std::move(cluster));
            }
        } else {
            // Per-cluster: one EdfScheduler + Cluster per ClockDomain
            for (std::size_t i = 0; i < platform.clock_domain_count(); ++i) {
                auto& cd = platform.clock_domain(i);
                auto proc_span = cd.processors();
                std::vector<core::Processor*> procs(proc_span.begin(), proc_span.end());
                if (procs.empty()) continue;

                auto sched = std::make_unique<algo::EdfScheduler>(engine, procs);
                double perf = procs[0]->type().performance();
                auto cluster =
                    std::make_unique<algo::Cluster>(cd, *sched, perf, ref_freq_max);
                cluster_ptrs.push_back(cluster.get());
                schedulers.push_back(std::move(sched));
                clusters.push_back(std::move(cluster));
            }
        }

        if (config.verbose) {
            std::cerr << "Built " << clusters.size() << " clusters ("
                      << config.granularity << ")" << std::endl;
        }

        // 6. Apply u_target to LITTLE clusters
        if (config.u_target.has_value()) {
            for (auto* c : cluster_ptrs) {
                if (c->perf() < 1.0) {
                    c->set_u_target(*config.u_target);
                }
            }
        }

        // 7. Configure scheduler policies (skip DVFS/reclamation in per-core mode)
        if (config.granularity == "per-core") {
            if (config.reclaim != "none") {
                std::cerr << "Warning: --reclaim " << config.reclaim
                          << " ignored in per-core mode" << std::endl;
            }
            if (config.dvfs != "none") {
                std::cerr << "Warning: --dvfs " << config.dvfs
                          << " ignored in per-core mode" << std::endl;
            }
        } else {
            for (auto& sched : schedulers) {
                if (config.reclaim == "grub") {
                    sched->enable_grub();
                } else if (config.reclaim == "cash") {
                    sched->enable_cash();
                }

                if (config.dvfs == "power-aware") {
                    sched->enable_power_aware_dvfs(
                        core::duration_from_seconds(config.dvfs_cooldown_ms / 1000.0));
                } else if (config.dvfs == "ffa") {
                    sched->enable_ffa(
                        core::duration_from_seconds(config.dvfs_cooldown_ms / 1000.0));
                } else if (config.dvfs == "csf") {
                    sched->enable_csf(
                        core::duration_from_seconds(config.dvfs_cooldown_ms / 1000.0));
                } else if (config.dvfs == "ffa-timer") {
                    sched->enable_ffa_timer(
                        core::duration_from_seconds(config.dvfs_cooldown_ms / 1000.0));
                } else if (config.dvfs == "csf-timer") {
                    sched->enable_csf_timer(
                        core::duration_from_seconds(config.dvfs_cooldown_ms / 1000.0));
                }
            }
        }

        // 8. Create allocator
        auto allocator = make_allocator(config, engine, cluster_ptrs);

        // 9. Set expected_total_util on adaptive allocators
        double total_util = 0.0;
        for (const auto& tp : scenario.tasks) {
            total_util += core::duration_ratio(tp.wcet, tp.period);
        }

        if (auto* adaptive =
                dynamic_cast<algo::FFCapAdaptiveLinearAllocator*>(allocator.get())) {
            adaptive->set_expected_total_util(total_util);
        } else if (auto* adaptive =
                       dynamic_cast<algo::FFCapAdaptivePolyAllocator*>(allocator.get())) {
            adaptive->set_expected_total_util(total_util);
        }

        // 10. Set up trace writer (in-memory for metrics)
        io::MemoryTraceWriter trace_writer;
        engine.set_trace_writer(&trace_writer);

        if (config.verbose) {
            std::cerr << "Running simulation with allocator: " << config.alloc << std::endl;
        }

        // 11. Run simulation
        engine.run();

        // 12. Compute result
        std::size_t result = 0;
        if (config.alloc == "counting") {
            result = dynamic_cast<algo::CountingAllocator&>(*allocator).allocation_count();
        } else {
            auto metrics = io::compute_metrics(trace_writer.records());
            result = metrics.rejected_tasks;
        }

        // 13. Print CSV line
        std::string label = config.alloc;
        if (config.alloc == "ff_cap" && config.u_target.has_value()) {
            label += "_" + std::to_string(*config.u_target);
        }
        std::cout << config.scenario_file << ";" << label << ";" << result << std::endl;

        return 0;
    }
    catch (const io::LoaderError& e) {
        std::cerr << "Config error: " << e.what() << std::endl;
        return 1;
    }
    catch (const algo::AdmissionError& e) {
        std::cerr << "Admission failed: " << e.what() << std::endl;
        return 2;
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
