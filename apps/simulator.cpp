#include <optional>
#include <protocols/hardware.hpp>
#include <protocols/scenario.hpp>
#include <protocols/traces.hpp>
#include <simulator/engine.hpp>
#include <simulator/event.hpp>
#include <simulator/factory.hpp>
#include <simulator/platform.hpp>
#include <simulator/task.hpp>

#include <cstdlib>
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef TRACY_ENABLE
#include <chrono>
#include <thread>
#include <tracy/Tracy.hpp>
#endif

namespace fs = std::filesystem;

struct AppConfig {
        fs::path output_file{"logs.json"};
        fs::path scenario_file{"scenario.json"};
        fs::path platform_file{"platform.json"};
        std::string sched;
        std::string alloc;
        bool active_delay{false};
        std::optional<double> u_target;
        std::unordered_map<std::string, std::string> alloc_args;
};

constexpr std::array<const char*, 6> policies{
    "grub - M-GRUB with global reclaiming",
    "pa   - M-GRUB-PA with global reclaiming",
    "ffa  - M-GRUB with minimum frequency",
    "csf  - M-GRUB with minimum active processor",
    "ffa_timer",
    "csf_timer"};

auto parse_args(const int argc, const char** argv) -> AppConfig
{
        AppConfig config;

        // clang-format off
	cxxopts::Options options("schedsim", "GRUB Scheduler Simulation for a Given Task Set and Platform");
	options.add_options()
	    ("h,help", "Show this help message.")
	    ("i,input", "Specify the scenario file.", cxxopts::value<std::string>())
	    ("p,platform", "Specify the platform configuration file.", cxxopts::value<std::string>())
	    ("a,alloc", "Specify the cluster allocator", cxxopts::value<std::string>())
	    ("A,alloc-arg", "Allocator argument in key=value form (repeatable).",
	        cxxopts::value<std::vector<std::string>>())
	    ("s,sched", "Specify the scheduling policy to be used.", cxxopts::value<std::string>())
            ("delay", "Activate delay during DVFS and DPM switch mode", cxxopts::value<bool>()->default_value("false"))
	    ("o,output", "Specify the output file to write the simulation results.", cxxopts::value<std::string>())
	    ("target", "Specify u_target for the LITTLE cluster", cxxopts::value<double>()->default_value("1"));
        // clang-format on
        const auto cli = options.parse(argc, argv);

        if (cli.count("help") || cli.arguments().empty()) {
                std::cout << options.help() << std::endl;
                exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        if (cli.count("input")) { config.scenario_file = cli["input"].as<std::string>(); }
        if (cli.count("platform")) { config.platform_file = cli["platform"].as<std::string>(); }
        if (cli.count("sched")) { config.sched = cli["sched"].as<std::string>(); }
        if (cli.count("alloc")) { config.alloc = cli["alloc"].as<std::string>(); }
        if (cli.count("alloc-arg")) {
                config.alloc_args =
                    parse_allocator_args(cli["alloc-arg"].as<std::vector<std::string>>());
        }
        if (cli.count("output")) { config.output_file = cli["output"].as<std::string>(); }
        if (cli.count("delay")) { config.active_delay = true; }
        if (cli.count("target")) { config.u_target = cli["target"].as<double>(); }

        return config;
}

auto main(const int argc, const char** argv) -> int
{
        using namespace std;

        const bool FREESCALING_ALLOWED{false};

        try {
                auto config = parse_args(argc, argv);

                // Create the simulation engine and attach a scheduler to it
                Engine sim(config.active_delay);

                auto taskset = protocols::scenario::read_file(config.scenario_file);
                auto PlatformConfig = protocols::hardware::read_file(config.platform_file);

                // Insert the platform configured through the scenario file
                auto plat = make_unique<Platform>(sim, FREESCALING_ALLOWED);
                auto* plat_ptr = plat.get();
                sim.platform(std::move(plat));

                auto alloc = select_alloc(config.alloc, sim, config.alloc_args);

                std::size_t cluster_id_cpt{1};
                for (const protocols::hardware::Cluster& clu : PlatformConfig.clusters) {
                        auto newclu = std::make_unique<Cluster>(
                            sim,
                            cluster_id_cpt,
                            clu.frequencies,
                            clu.effective_freq,
                            clu.perf_score,
                            (clu.perf_score < 1 && config.u_target.has_value()
                                 ? config.u_target.value()
                                 : clu.perf_score));
                        auto* clu_ptr = newclu.get();
                        clu_ptr->create_procs(clu.nb_procs);

                        auto sched = select_sched(config.sched, sim);

                        alloc->add_child_sched(clu_ptr, std::move(sched));
                        plat_ptr->add_cluster(std::move(newclu));
                        cluster_id_cpt++;
                }

                sim.scheduler(std::move(alloc));

                std::vector<std::unique_ptr<Task>> tasks;
                tasks.reserve(taskset.tasks.size());

                // Create tasks and job arrival events
                for (const auto& input_task : taskset.tasks) {
                        auto new_task = make_unique<Task>(
                            sim, input_task.id, input_task.period, input_task.utilization);
                        auto* task_ptr = new_task.get();

                        // For each job of tasks add a "job arrival" event in the future list
                        for (const auto& job : input_task.jobs) {
                                sim.add_event(
                                    events::JobArrival{
                                        .task_of_job = task_ptr, .job_duration = job.duration},
                                    job.arrival);
                        }
                        tasks.push_back(std::move(new_task));
                }

                // Simulate the system (job set + platform) with the chosen scheduler
                sim.simulation();

                protocols::traces::write_log_file(sim.traces(), config.output_file);

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
