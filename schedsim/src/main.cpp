#include <cstdlib>
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <ostream>
#include <protocols/hardware.hpp>
#include <protocols/scenario.hpp>
#include <protocols/traces.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
#include "platform.hpp"
#include "scheduler.hpp"
#include "schedulers/csf.hpp"
#include "schedulers/ffa.hpp"
#include "schedulers/parallel.hpp"
#include "schedulers/power_aware.hpp"
#include "task.hpp"
#include <version.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace fs = std::filesystem;

struct app_config {
        fs::path output_file{"logs.json"};
        fs::path scenario_file{"scenario.json"};
        fs::path platform_file{"platform.json"};
        std::string sched;
};

constexpr std::array<const char*, 4> policies{
    "grub - M-GRUB with global reclaiming",
    "pa   - M-GRUB-PA with global reclaiming",
    "ffa  - M-GRUB with minimum frequency",
    "csf  - M-GRUB with minimum active processor"};

auto parse_args(const int argc, const char** argv) -> app_config
{
        app_config config;

        // clang-format off
	cxxopts::Options options("schedsim", "GRUB Scheduler Simulation for a Given Task Set and Platform");
	options.add_options()
	        ("h,help", "Show this help message.")
                ("v,version", "Show the build version")
		("s,scenario", "Specify the scenario file.", cxxopts::value<std::string>())
		("p,platform", "Specify the platform configuration file.", cxxopts::value<std::string>())
		("sched", "Specify the scheduling policy to be used.", cxxopts::value<std::string>())
		("scheds", "List the available schedulers.", cxxopts::value<bool>()->default_value("false"))
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
                auto platform_config = protocols::hardware::read_file(config.platform_file);

                // Create the simulation engine and attache to it a scheduler
                auto sim = make_shared<engine>();

                // Insert the platform configured through the scenario file, in the simulation
                // engine
                auto plat = make_shared<platform>(
                    sim,
                    platform_config.nb_procs,
                    platform_config.frequencies,
                    platform_config.effective_freq,
                    FREESCALING_ALLOWED);
                sim->set_platform(plat);

                std::shared_ptr<scheduler> sched;
                if (config.sched == "grub") { sched = make_shared<sched_parallel>(sim); }
                else if (config.sched == "pa") {
                        sched = make_shared<sched_power_aware>(sim);
                }
                else if (config.sched == "ffa") {
                        sched = make_shared<ffa>(sim);
                }
                else if (config.sched == "csf") {
                        sched = make_shared<csf>(sim);
                }
                else {
                        throw std::invalid_argument("Undefined scheduling policy");
                }
                sim->set_scheduler(sched);

                std::vector<std::shared_ptr<task>> tasks{taskset.tasks.size()};

                // Create tasks and job arrival events
                for (auto input_task : taskset.tasks) {
                        auto new_task = make_shared<task>(
                            sim, input_task.id, input_task.period, input_task.utilization);

                        // For each job of tasks add a "job arrival" event in the future list
                        for (auto job : input_task.jobs) {
                                sim->add_event(
                                    events::job_arrival{new_task, job.duration}, job.arrival);
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
