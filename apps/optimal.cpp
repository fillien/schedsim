#include "analyzers/stats.hpp"
#include <__ostream/print.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>
#include <protocols/hardware.hpp>
#include <protocols/scenario.hpp>
#include <protocols/traces.hpp>
#include <random>
#include <simulator/allocator.hpp>
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

#include <cstdlib>
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cstdint>
#include <fstream>
#include <sstream>

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

enum alloc { SCHED1, SCHED2 };

struct Node {
        Node* parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
        alloc allocation;
        std::size_t nb_rejects = 0;
        std::size_t nb_visit = 0;
        bool leaf = false;
        double last_score;
        std::size_t last_reject;
};

// Helper: Convert alloc to string
std::string alloc_to_string(alloc a)
{
        switch (a) {
        // case REJECT: return "REJECT";
        case SCHED1: return "SCHED1";
        case SCHED2: return "SCHED2";
        default: return "UNKNOWN";
        }
}

// Helper to create a unique node id string
std::string node_id(const Node* node)
{
        std::ostringstream oss;
        oss << "N" << reinterpret_cast<std::uintptr_t>(node);
        return oss.str();
}

// Write a node and its children recursively
void writeDot(const Node* node, std::ofstream& f)
{
        if (!node) return;

        f << "    " << node_id(node) << " [label=\"nb_visit=" << node->nb_visit
          << "\\nnb_rejects=" << node->nb_rejects
          << "\\nalloc=" << alloc_to_string(node->allocation) << "\\nscore=" << node->last_score
          << "\"];\n";

        for (const auto& child : node->children) {
                f << "    " << node_id(node) << " -> " << node_id(child.get()) << ";\n";
                writeDot(child.get(), f);
        }
}

// Main entry point to write DOT file
void treeToDot(const Node* root, const std::string& filename)
{
        std::ofstream f(filename);
        f << "digraph Tree {\n";
        writeDot(root, f);
        f << "}\n";
}

auto simulate(
    const AppConfig& config,
    const auto taskset,
    const auto platconfig,
    const std::vector<unsigned>& pattern) -> std::pair<std::size_t, std::size_t>
{
        using namespace std;
        std::shared_ptr<Engine> sim = make_shared<Engine>(config.active_delay);
        auto plat = make_shared<Platform>(sim, false);
        sim->platform(plat);

        auto alloc = make_shared<allocators::MCTS>(sim, pattern);

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

                auto sched = make_shared<scheds::Parallel>(sim);
                alloc->add_child_sched(newclu, sched);
                plat->add_cluster(newclu);
                cluster_id_cpt++;
        }

        sim->scheduler(alloc);
        vector<shared_ptr<Task>> tasks{taskset.tasks.size()};

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
        std::vector<std::pair<double, protocols::traces::trace>> log(
            sim->traces().begin(), sim->traces().end());

        return {outputs::stats::count_rejected(log), alloc->get_nb_alloc()};
}

auto selection(Node* root) -> Node*
{
        // const auto score = [](const std::unique_ptr<Node>& node) {
        //         const double p_visite = (node->parent != nullptr ? node->parent->nb_visit : 1);
        //         const double nb_visit = static_cast<double>(node->nb_visit);
        //         const double c = std::numbers::sqrt2;
        //         const double exploration = c * std::sqrt(std::log(p_visite) / nb_visit);
        //         const double avg_reject = (static_cast<double>(node->nb_rejects) / nb_visit);
        //         return avg_reject + exploration;
        // };
        const auto score = [](const std::unique_ptr<Node>& node) {
                if (node->nb_visit == 0) {
                        // return std::numeric_limits<double>::infinity();
                        return 0.0;
                }
                const auto nb_visit = static_cast<double>(node->nb_visit);
                const double avg_rejects = static_cast<double>(node->nb_rejects) / nb_visit;
                const double c = 5 * std::numbers::sqrt2;
                // const double c = 5.0;
                const double parent_visits = (node->parent != nullptr) ? node->parent->nb_visit : 1;
                double exploration = c * std::sqrt(std::log(parent_visits) / node->nb_visit);
                node->last_score = avg_rejects - exploration;
                return avg_rejects - exploration; // if LOWER rejects is better
        };
        Node* current = root;
        while (!current->children.empty()) {
                // For each children select the one with the bigger score
                auto best = std::ranges::min_element(
                    current->children, [&score](const auto& first, const auto& second) {
                            return score(first) < score(second);
                    });
                current = best->get();
        }
        return current;
}

auto get_random_alloc() -> std::size_t
{
        static std::mt19937 rng(static_cast<std::size_t>(std::time(nullptr)));
        std::uniform_int_distribution<int> dist(0, 1);
        return static_cast<std::size_t>(dist(rng));
}

auto add_nodes(Node* parent) -> void
{
        for (std::size_t i = 0; i < 2; ++i) {
                auto child = std::make_unique<Node>();
                child->parent = parent;
                child->allocation = alloc(i);
                parent->children.push_back(std::move(child));
        }
}

auto get_first_pattern(Node* node) -> std::vector<unsigned>
{
        std::vector<unsigned> pattern;
        Node* current = node;
        while (current->parent != nullptr) {
                pattern.push_back(current->allocation);
                current = current->parent;
        }
        return pattern;
}

auto backpropagate(Node* node, std::size_t score) -> void
{
        Node* current = node;
        while (current->parent != nullptr) {
                current->nb_rejects += score;
                current->nb_visit++;
                current = current->parent;
        }
        current->nb_rejects += score;
        current->nb_visit++;
}

auto run_monte_carlo(auto config, auto taskset, auto plat) -> void
{
        Node tree;
        Node* current_node = &tree;

        double best = std::numeric_limits<double>::infinity();
        std::size_t nb_leaf_found = 0;

        const int MAX_ITR = 30'000'000;
        // const int MAX_ITR = 3001;

        for (int i = 0; i < MAX_ITR; ++i) {
                current_node = selection(&tree);

                if (current_node->leaf) {
                        auto pattern = get_first_pattern(current_node);
                        auto score = current_node->last_reject;
                        backpropagate(current_node, score);
                }
                else {
                        add_nodes(current_node);
                        const auto branch = get_random_alloc();
                        current_node = current_node->children.at(branch).get();

                        auto pattern = get_first_pattern(current_node);
                        auto [score, nb_alloc] = simulate(config, taskset, plat, pattern);

                        if (nb_alloc <= pattern.size()) {
                                if (!current_node->leaf) { nb_leaf_found++; }
                                current_node->leaf = true;
                                current_node->last_reject = score;
                                const double ratio =
                                    static_cast<double>(score) / static_cast<double>(nb_alloc);
                                if (ratio < best) {
                                        best = ratio;
                                        std::println("new best: {}", best);
                                }
                        }
                        backpropagate(current_node, score);
                }


                if (!(i % 10'000)) {
                        std::println("{}/{} ; leafs found = {} ; best = {}", i, MAX_ITR, nb_leaf_found, best);
                }
                // std::println("{0}", pattern);
        }

        std::println("best: {}", best);

        // treeToDot(&tree, "tree.dot");
}

auto main(const int argc, const char** argv) -> int
{
        using namespace std;

        const bool FREESCALING_ALLOWED{false};

        try {
                const auto config = parse_args(argc, argv);
                const auto taskset = protocols::scenario::read_file(config.scenario_file);
                const auto plat = protocols::hardware::read_file(config.platform_file);

                run_monte_carlo(config, taskset, plat);

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
