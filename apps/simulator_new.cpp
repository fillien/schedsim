#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/types.hpp>

#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/single_scheduler_allocator.hpp>

#include <schedsim/io/error.hpp>
#include <schedsim/io/platform_loader.hpp>
#include <schedsim/io/scenario_injection.hpp>
#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/io/trace_writers.hpp>

#include <cxxopts.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace core = schedsim::core;
namespace algo = schedsim::algo;
namespace io = schedsim::io;

struct Config {
    std::string scenario_file;
    std::string platform_file;
    std::string scheduler{"edf"};
    std::string reclaim{"none"};
    std::string dvfs{"none"};
    double dvfs_cooldown_ms{0.0};
    std::string dpm{"none"};
    int dpm_cstate{1};
    double duration{0.0};  // 0 = auto
    bool energy{false};
    bool context_switch{false};
    std::string output_file{"-"};
    std::string format{"json"};
    bool metrics{false};
    bool verbose{false};
};

Config parse_args(int argc, char** argv) {
    cxxopts::Options options("schedsim-new", "Real-time scheduler simulator");

    options.add_options()
        ("i,input", "Scenario file (JSON)", cxxopts::value<std::string>())
        ("p,platform", "Platform configuration (JSON)", cxxopts::value<std::string>())
        ("s,scheduler", "Scheduler: edf (default: edf)", cxxopts::value<std::string>()->default_value("edf"))
        ("reclaim", "Reclamation: none|grub|cash (default: none)", cxxopts::value<std::string>()->default_value("none"))
        ("dvfs", "DVFS: none|power-aware (default: none)", cxxopts::value<std::string>()->default_value("none"))
        ("dvfs-cooldown", "DVFS cooldown in ms (default: 0)", cxxopts::value<double>()->default_value("0"))
        ("dpm", "DPM: none|basic (default: none)", cxxopts::value<std::string>()->default_value("none"))
        ("dpm-cstate", "Target C-state (default: 1)", cxxopts::value<int>()->default_value("1"))
        ("d,duration", "Simulation duration in seconds (default: auto)", cxxopts::value<double>()->default_value("0"))
        ("energy", "Enable energy tracking")
        ("context-switch", "Enable context switch overhead")
        ("o,output", "Trace output (default: stdout)", cxxopts::value<std::string>()->default_value("-"))
        ("format", "Format: json|null (default: json)", cxxopts::value<std::string>()->default_value("json"))
        ("metrics", "Print metrics to stderr")
        ("v,verbose", "Verbose output")
        ("h,help", "Show help");

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

    Config config;
    config.scenario_file = result["input"].as<std::string>();
    config.platform_file = result["platform"].as<std::string>();
    config.scheduler = result["scheduler"].as<std::string>();
    config.reclaim = result["reclaim"].as<std::string>();
    config.dvfs = result["dvfs"].as<std::string>();
    config.dvfs_cooldown_ms = result["dvfs-cooldown"].as<double>();
    config.dpm = result["dpm"].as<std::string>();
    config.dpm_cstate = result["dpm-cstate"].as<int>();
    config.duration = result["duration"].as<double>();
    config.energy = result.count("energy") != 0U;
    config.context_switch = result.count("context-switch") != 0U;
    config.output_file = result["output"].as<std::string>();
    config.format = result["format"].as<std::string>();
    config.metrics = result.count("metrics") != 0U;
    config.verbose = result.count("verbose") != 0U;

    return config;
}

} // anonymous namespace

int main(int argc, char** argv) {
    try {
        auto config = parse_args(argc, argv);

        if (config.verbose) {
            std::cerr << "Loading platform from: " << config.platform_file << std::endl;
            std::cerr << "Loading scenario from: " << config.scenario_file << std::endl;
        }

        // 1. Create engine
        core::Engine engine;

        // 2. Load platform
        io::load_platform(engine, config.platform_file);

        // 3. Load scenario and inject tasks (before finalize)
        auto scenario = io::load_scenario(config.scenario_file);
        io::inject_scenario(engine, scenario);

        // 4. Enable optional features (before finalize)
        if (config.energy) {
            engine.enable_energy_tracking(true);
        }
        if (config.context_switch) {
            engine.enable_context_switch(true);
        }

        // 5. Schedule job arrivals (before engine finalize, but tasks already created)
        for (std::size_t i = 0; i < scenario.tasks.size(); ++i) {
            io::schedule_arrivals(engine, engine.platform().task(i), scenario.tasks[i].jobs);
        }

        // 6. Finalize platform (after scheduling arrivals)
        engine.platform().finalize();

        // 7. Create scheduler with all processors
        std::vector<core::Processor*> procs;
        procs.reserve(engine.platform().processor_count());
        for (std::size_t i = 0; i < engine.platform().processor_count(); ++i) {
            procs.push_back(&engine.platform().processor(i));
        }
        algo::EdfScheduler scheduler(engine, procs);

        // 8. Create CBS servers for each task (REQUIRED)
        for (std::size_t i = 0; i < engine.platform().task_count(); ++i) {
            scheduler.add_server(engine.platform().task(i));
        }

        // 9. Configure policies
        if (config.reclaim == "grub") {
            scheduler.enable_grub();
        } else if (config.reclaim == "cash") {
            scheduler.enable_cash();
        }

        if (config.dvfs == "power-aware") {
            scheduler.enable_power_aware_dvfs(
                core::Duration{config.dvfs_cooldown_ms / 1000.0});
        }

        if (config.dpm == "basic") {
            scheduler.enable_basic_dpm(config.dpm_cstate);
        }

        // 10. Create allocator (automatically sets job arrival handler)
        algo::SingleSchedulerAllocator allocator(engine, scheduler);

        // 11. Setup trace writer
        std::unique_ptr<core::TraceWriter> writer;
        std::ofstream outfile;

        if (config.format == "null") {
            writer = std::make_unique<io::NullTraceWriter>();
        } else if (config.output_file == "-") {
            writer = std::make_unique<io::JsonTraceWriter>(std::cout);
        } else {
            outfile.open(config.output_file);
            if (!outfile) {
                std::cerr << "Error: cannot open output file: " << config.output_file << std::endl;
                return 1;
            }
            writer = std::make_unique<io::JsonTraceWriter>(outfile);
        }
        engine.set_trace_writer(writer.get());

        if (config.verbose) {
            std::cerr << "Starting simulation..." << std::endl;
        }

        // 12. Run simulation
        if (config.duration > 0) {
            engine.run(core::TimePoint{core::Duration{config.duration}});
        } else {
            engine.run();
        }

        // 13. Finalize output
        if (auto* json = dynamic_cast<io::JsonTraceWriter*>(writer.get())) {
            json->finalize();
        }

        if (config.verbose) {
            std::cerr << "Simulation complete at time: "
                      << engine.time().time_since_epoch().count() << "s" << std::endl;
        }

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
