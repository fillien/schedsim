#include <cstdlib>
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
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
#include "schedulers/parallel.hpp"
#include "schedulers/power_aware.hpp"
#include "schedulers/power_aware_f_min.hpp"
#include "schedulers/power_aware_m_min.hpp"
#include "task.hpp"

namespace fs = std::filesystem;

struct app_config {
        fs::path output_file{"logs.json"};
        fs::path scenario_file{"scenario.json"};
        fs::path platform_file{"platform.json"};
        std::string policy;
};

constexpr std::array<const char*, 4> policies{
    "grub     - M-GRUB with global reclaiming",
    "pa       - M-GRUB-PA with global reclaiming",
    "pa_f_min - M-GRUB with minimum frequency",
    "pa_m_min - M-GRUB with minimum active processor"};

auto parse_args(const int argc, const char** argv) -> app_config
{
        app_config config;

        // clang-format off
	cxxopts::Options options("schedsim", "Simulate the execution of the GRUB scheduler for a given taskset on given platform");
	options.add_options()
		("h,help", "Print this help message")
		("s,scenario", "Specify the scenario file", cxxopts::value<std::string>())
		("platform", "Specify the platform configuration file", cxxopts::value<std::string>()->default_value("platform.json"))
		("p,policy", "Specify the scheduling policy", cxxopts::value<std::string>())
		("policies", "List the available schedulers", cxxopts::value<bool>()->default_value("false"))
		("o,output", "Specify the output file", cxxopts::value<std::string>());
        // clang-format on
        const auto cli = options.parse(argc, argv);

        if (cli.count("help") || cli.arguments().empty()) {
                std::cout << options.help() << std::endl;
                exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        if (cli.count("policies")) {
                std::cout << "Available schedulers:\n";
                for (const auto& policy : policies) {
                        std::cout << '\t' << policy << '\n';
                }
        }
        if (cli.count("scenario")) { config.scenario_file = cli["scenario"].as<std::string>(); }
        if (cli.count("platform")) { config.platform_file = cli["platform"].as<std::string>(); }
        if (cli.count("policy")) { config.policy = cli["policy"].as<std::string>(); }
        if (cli.count("output")) { config.output_file = cli["output"].as<std::string>(); }

        return config;
}

auto main(const int argc, const char** argv) -> int
{
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
                if (config.policy == "grub") { sched = make_shared<sched_parallel>(sim); }
                else if (config.policy == "pa") {
                        sched = make_shared<sched_power_aware>(sim);
                }
                else if (config.policy == "pa_f_min") {
                        sched = make_shared<pa_f_min>(sim);
                }
                else if (config.policy == "pa_m_min") {
                        sched = make_shared<pa_m_min>(sim);
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
