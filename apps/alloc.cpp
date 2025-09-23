#include <analyzers/stats.hpp>
#include <protocols/hardware.hpp>
#include <protocols/scenario.hpp>
#include <protocols/traces.hpp>
#include <simulator/allocator.hpp>
#include <simulator/allocators/ff_big_first.hpp>
#include <simulator/allocators/ff_cap.hpp>
#include <simulator/allocators/ff_lb.hpp>
#include <simulator/allocators/ff_little_first.hpp>
#include <simulator/allocators/ff_sma.hpp>
#include <simulator/allocators/mcts.hpp>
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

#include <cstddef>
#include <cstdlib>
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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

auto parse_allocator_args(const std::vector<std::string>& raw_args)
    -> std::unordered_map<std::string, std::string>
{
        std::unordered_map<std::string, std::string> result;
        for (const auto& arg : raw_args) {
                const auto pos = arg.find('=');
                if (pos == std::string::npos) {
                        throw std::invalid_argument(
                            "Allocator arguments must follow the key=value format");
                }

                auto key = arg.substr(0, pos);
                auto value = arg.substr(pos + 1);

                if (key.empty() || value.empty()) {
                        throw std::invalid_argument(
                            "Allocator arguments require both a non-empty key and value");
                }

                if (!result.emplace(std::move(key), std::move(value)).second) {
                        throw std::invalid_argument("Duplicate allocator argument: " + arg);
                }
        }

        return result;
}

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
        if (cli.count("target")) { config.u_target = cli["target"].as<double>(); }

        return config;
}

auto select_alloc(
    const std::string& choice,
    const std::shared_ptr<Engine>& sim,
    const std::unordered_map<std::string, std::string>& alloc_args)
    -> std::shared_ptr<allocators::Allocator>
{
        using namespace allocators;

        const auto ensure_allowed_args = [&](std::initializer_list<const char*> allowed_keys) {
                std::unordered_set<std::string> allowed;
                allowed.reserve(allowed_keys.size());
                for (const auto* key : allowed_keys) {
                        allowed.emplace(key);
                }

                for (const auto& [key, value] : alloc_args) {
                        if (allowed.find(key) == allowed.end()) {
                                throw std::invalid_argument(
                                    "Undefined allocator argument '" + key + "' for policy '" +
                                    choice + "'");
                        }
                }
        };

        // if (choice.empty() || choice == "default") { return std::make_shared<Allocator>(sim); }
        if (choice == "ff_big_first") {
                ensure_allowed_args({});
                return std::make_shared<FFBigFirst>(sim);
        }
        if (choice == "ff_little_first") {
                ensure_allowed_args({});
                return std::make_shared<FFLittleFirst>(sim);
        }
        if (choice == "ff_cap") {
                ensure_allowed_args({});
                return std::make_shared<FFCap>(sim);
        }
        if (choice == "ff_lb") {
                ensure_allowed_args({});
                return std::make_shared<FirstFitLoadBalancer>(sim);
        }
        if (choice == "ff_sma") {
                ensure_allowed_args({"sample_rate", "num_samples"});

                double sample_rate = 0.5;
                if (const auto it = alloc_args.find("sample_rate"); it != alloc_args.end()) {
                        try {
                                sample_rate = std::stod(it->second);
                        }
                        catch (const std::exception& e) {
                                throw std::invalid_argument(
                                    "Invalid value for ff_sma sample_rate: " + it->second);
                        }
                }

                int num_samples = 5;
                if (const auto it = alloc_args.find("num_samples"); it != alloc_args.end()) {
                        try {
                                num_samples = std::stoi(it->second);
                        }
                        catch (const std::exception& e) {
                                throw std::invalid_argument(
                                    "Invalid value for ff_sma num_samples: " + it->second);
                        }
                }

                return std::make_shared<FFSma>(sim, sample_rate, num_samples);
        }
        throw std::invalid_argument("Undefined allocation policy");
}

auto select_sched(const std::string& choice, const std::shared_ptr<Engine>& sim)
    -> std::shared_ptr<scheds::Scheduler>
{
        using namespace scheds;
        if (choice.empty() || choice == "grub") { return std::make_shared<Parallel>(sim); }
        if (choice == "pa") { return std::make_shared<PowerAware>(sim); }
        if (choice == "ffa") { return std::make_shared<Ffa>(sim); }
        if (choice == "csf") { return std::make_shared<Csf>(sim); }
        if (choice == "ffa_timer") { return std::make_shared<FfaTimer>(sim); }
        if (choice == "csf_timer") { return std::make_shared<CsfTimer>(sim); }
        throw std::invalid_argument("Undefined scheduling policy");
}

auto main(const int argc, const char** argv) -> int
{
        using namespace std;

        const bool FREESCALING_ALLOWED{false};

        try {
                const auto config = parse_args(argc, argv);
                const auto taskset = protocols::scenario::read_file(config.scenario_file);
                const auto plat_config = protocols::hardware::read_file(config.platform_file);

                std::shared_ptr<Engine> sim = make_shared<Engine>(config.active_delay);
                auto plat = make_shared<Platform>(sim, FREESCALING_ALLOWED);
                sim->platform(plat);
                auto alloc = select_alloc(config.alloc, sim, config.alloc_args);
                // auto alloc = std::make_shared<allocators::FFLittleFirst>(sim);

                std::size_t cluster_id_cpt{1};
                for (const protocols::hardware::Cluster& clu : plat_config.clusters) {
                        auto newclu = std::make_shared<Cluster>(
                            sim,
                            cluster_id_cpt,
                            clu.frequencies,
                            clu.effective_freq,
                            clu.perf_score,
                            (clu.perf_score < 1 && config.u_target.has_value()
                                 ? config.u_target.value()
                                 : clu.perf_score));
                        newclu->create_procs(clu.nb_procs);
                        auto sched = select_sched(config.sched, sim);
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

                std::print("simulate...");
                sim->simulation();
                std::println("OK");

                auto result = outputs::stats::count_rejected(sim->traces());
                // std::cout << alloc->get_nb_alloc() << std::endl;

                std::ofstream datafile("min_taskset_result.csv", std::ios::app);
                if (!datafile) {
                        std::cerr << "failed to open alloc-result.csv file" << std::endl;
                        return EXIT_FAILURE;
                }
                if (config.alloc == "ff_cap") {
                        std::string algo = config.alloc;
                        if (config.u_target.has_value()) {
                                algo += "_" + std::to_string(config.u_target.value());
                        }
                        datafile << config.scenario_file << ";" << algo << ";" << result
                                 << std::endl;
                }
                else {
                        datafile << config.scenario_file << ";" << config.alloc << ";" << result
                                 << std::endl;
                }
                datafile.close();

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
