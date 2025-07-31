#include <cstddef>
#include <optional>
#include <protocols/hardware.hpp>
#include <protocols/scenario.hpp>
#include <protocols/traces.hpp>
#include <simulator/allocator.hpp>
#include <simulator/allocators/ff_big_first.hpp>
#include <simulator/allocators/ff_cap.hpp>
#include <simulator/allocators/ff_lb.hpp>
#include <simulator/allocators/ff_little_first.hpp>
#include <simulator/allocators/optimal.hpp>
#include <simulator/engine.hpp>
#include <simulator/entity.hpp>
#include <simulator/event.hpp>
#include <simulator/platform.hpp>
#include <simulator/scheduler.hpp>
#include <simulator/schedulers/csf.hpp>
#include <simulator/schedulers/csf_timer.hpp>
#include <simulator/schedulers/ffa.hpp>
#include <simulator/schedulers/ffa_timer.hpp>
#include <simulator/schedulers/parallel.hpp>
#include <simulator/schedulers/power_aware.hpp>
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
};

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
	    ("s,sched", "Specify the scheduling policy to be used.", cxxopts::value<std::string>())
	    ("o,output", "Specify the output file to write the simulation results.", cxxopts::value<std::string>());
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
        if (cli.count("output")) { config.output_file = cli["output"].as<std::string>(); }

        return config;
}

auto explore(const AppConfig& config, const auto taskset, const auto platconfig, void* exchange) -> void*
{
        using namespace std;
        std::shared_ptr<Engine> sim = make_shared<Engine>(config.active_delay);
        auto plat = make_shared<Platform>(sim, false);
        sim->platform(plat);

        auto alloc = make_shared<allocators::Optimal>(sim, exchange);

        std::size_t cluster_id_cpt{1};
        for (const protocols::hardware::Cluster& clu : platconfig.clusters) {
                auto newclu = make_shared<Cluster>(
                    sim,
                    cluster_id_cpt,
                    clu.frequencies,
                    clu.effective_freq,
                    clu.perf_score,
                    (clu.perf_score < 1 && config.u_target.has_value() ? config.u_target.value()
                                                                       : clu.perf_score));
                newclu->create_procs(clu.nb_procs);

                auto sched = std::make_shared<scheds::Parallel>(sim);
                alloc->add_child_sched(newclu, sched);
                plat->add_cluster(newclu);
                cluster_id_cpt++;
        }

        sim->scheduler(alloc);
        std::vector<std::shared_ptr<Task>> tasks{taskset.tasks.size()};

        for (auto input_task : taskset.tasks) {
                auto new_task = make_shared<Task>(
                    sim, input_task.id, input_task.period, input_task.utilization);

                for (auto job : input_task.jobs) {
                        sim->add_event(
                            events::JobArrival{
                                .task_of_job = new_task, .job_duration = job.duration},
                            job.arrival);
                }
                tasks.push_back(std::move(new_task));
        }

        sim->simulation();
        protocols::traces::write_log_file(sim->traces(), config.output_file);
        return alloc->get_exchange();
}

auto main(const int argc, const char** argv) -> int
{
        using namespace std;

        const bool FREESCALING_ALLOWED{false};

        try {
                const auto config = parse_args(argc, argv);
                const auto taskset = protocols::scenario::read_file(config.scenario_file);
                const auto PlatformConfig = protocols::hardware::read_file(config.platform_file);

                void * exchange = nullptr;

                while (true) {
                        exchange = explore(config, taskset, PlatformConfig, exchange);
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
