#include <allocator.hpp>
#include <allocators/high_perf_first.hpp>
#include <allocators/low_perf_first.hpp>
#include <allocators/smart_ass.hpp>
#include <cstdlib>
#include <cxxopts.hpp>
#include <engine.hpp>
#include <entity.hpp>
#include <event.hpp>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <ostream>
#include <platform.hpp>
#include <protocols/hardware.hpp>
#include <protocols/scenario.hpp>
#include <protocols/traces.hpp>
#include <scheduler.hpp>
#include <schedulers/csf.hpp>
#include <schedulers/csf_timer.hpp>
#include <schedulers/ffa.hpp>
#include <schedulers/ffa_timer.hpp>
#include <schedulers/parallel.hpp>
#include <schedulers/power_aware.hpp>
#include <stdexcept>
#include <string>
#include <task.hpp>
#include <vector>
#include <version.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace fs = std::filesystem;

struct AppConfig {
        fs::path output_file{"logs.json"};
        fs::path scenario_file{"scenario.json"};
        fs::path platform_file{"platform.json"};
        std::string sched;
        bool active_delay;
};

constexpr std::array<const char*, 6> policies{
    "grub - M-GRUB with global reclaiming",
    "pa   - M-GRUB-PA with global reclaiming",
    "ffa  - M-GRUB with minimum frequency",
    "csf  - M-GRUB with minimum active processor"
    "ffa_timer",
    "csf_timer"};

auto parse_args(const int argc, const char** argv) -> AppConfig
{
        AppConfig config;

        // clang-format off
	cxxopts::Options options("schedsim", "GRUB Scheduler Simulation for a Given Task Set and Platform");
	options.add_options()
	    ("h,help", "Show this help message.")
	    ("v,version", "Show the build version")
	    ("s,scenario", "Specify the scenario file.", cxxopts::value<std::string>())
	    ("p,platform", "Specify the platform configuration file.", cxxopts::value<std::string>())
	    ("sched", "Specify the scheduling policy to be used.", cxxopts::value<std::string>())
	    ("scheds", "List the available schedulers.", cxxopts::value<bool>()->default_value("false"))
            ("delay", "Activate delay during DVFS and DPM switch mode", cxxopts::value<bool>()->default_value("false"))
	    ("o,output", "Specify the output file to write the simulation results.", cxxopts::value<std::string>());
        // clang-format on
        const auto cli = options.parse(argc, argv);

        if (cli.count("help") || cli.count("version") || cli.arguments().empty()) {
                if (cli.count("help")) { std::cout << options.help() << std::endl; }
                if (cli.count("version")) { std::cout << GIT_COMMIT_HASH << std::endl; }
                exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        if (cli.count("scheds")) {
                std::cout << "Available schedulers:\n";
                for (const auto& policy : policies) {
                        std::cout << '\t' << policy << '\n';
                }
        }
        if (cli.count("scenario")) { config.scenario_file = cli["scenario"].as<std::string>(); }
        if (cli.count("platform")) { config.platform_file = cli["platform"].as<std::string>(); }
        if (cli.count("sched")) { config.sched = cli["sched"].as<std::string>(); }
        if (cli.count("output")) { config.output_file = cli["output"].as<std::string>(); }
        if (cli.count("delay")) { config.active_delay = true; }

        return config;
}

auto main(const int argc, const char** argv) -> int
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        using namespace std;

        const bool FREESCALING_ALLOWED{false};

        try {
                auto config = parse_args(argc, argv);

                auto taskset = protocols::scenario::read_file(config.scenario_file);
                auto PlatformConfig = protocols::hardware::read_file(config.platform_file);

                // Create the simulation engine and attache to it a scheduler
                std::shared_ptr<Engine> sim = make_shared<Engine>(config.active_delay);

                // Insert the platform configured through the scenario file, in the simulation
                // engine
                auto plat = make_shared<Platform>(sim, FREESCALING_ALLOWED);
                sim->platform(plat);

                std::shared_ptr<allocators::Allocator> alloc =
                    std::make_shared<allocators::HighPerfFirst>(sim);

                std::size_t cluster_id_cpt{1};
                for (const protocols::hardware::Cluster& clu : PlatformConfig.clusters) {
                        auto newclu = std::make_shared<Cluster>(
                            sim,
                            cluster_id_cpt,
                            clu.frequencies,
                            clu.effective_freq,
                            clu.perf_score);
                        newclu->create_procs(clu.nb_procs);

                        std::shared_ptr<scheds::Scheduler> sched;

                        if (config.sched == "grub") { sched = make_shared<scheds::Parallel>(sim); }
                        else if (config.sched == "pa") {
                                sched = make_shared<scheds::PowerAware>(sim);
                        }
                        else if (config.sched == "ffa") {
                                sched = make_shared<scheds::Ffa>(sim);
                        }
                        else if (config.sched == "csf") {
                                sched = make_shared<scheds::Csf>(sim);
                        }
                        else if (config.sched == "ffa_timer") {
                                sched = make_shared<scheds::FfaTimer>(sim);
                        }
                        else if (config.sched == "csf_timer") {
                                sched = make_shared<scheds::CsfTimer>(sim);
                        }
                        else {
                                throw std::invalid_argument("Undefined scheduling policy");
                        }

                        alloc->add_child_sched(newclu, sched);
                        plat->add_cluster(newclu);
                        cluster_id_cpt++;
                }

                sim->scheduler(alloc);

                std::vector<std::shared_ptr<Task>> tasks{taskset.tasks.size()};

                // Create tasks and job arrival events
                for (auto input_task : taskset.tasks) {
                        auto new_task = make_shared<Task>(
                            sim, input_task.id, input_task.period, input_task.utilization);

                        // For each job of tasks add a "job arrival" event in the future list
                        for (auto job : input_task.jobs) {
                                sim->add_event(
                                    events::JobArrival{new_task, job.duration}, job.arrival);
                        }
                        tasks.push_back(std::move(new_task));
                }

                // Simulate the system (job set + platform) with the chosen scheduler
                sim->simulation();

                protocols::traces::write_log_file(sim->traces(), config.output_file);
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
