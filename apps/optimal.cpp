#include "analyzers/stats.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <ios>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <print>
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
#include <stack>

#include <atomic>
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
#include <omp.h>
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
};

auto simulate(
    const AppConfig& config,
    const auto& taskset,
    const auto& platconfig,
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
        const auto score = [](const std::unique_ptr<Node>& node) {
                if (node->nb_visit == 0) { return 0.0; }
                const auto nb_visit = static_cast<double>(node->nb_visit);
                const double avg_rejects = static_cast<double>(node->nb_rejects) / nb_visit;
                // const double c = 10 * std::numbers::sqrt2;
                constexpr double c = 1 * std::numbers::sqrt2;
                const double parent_visits =
                    (node == nullptr) ? static_cast<double>(node->parent->nb_visit) : 1.0;
                double exploration =
                    c * std::sqrt(std::log(parent_visits) / static_cast<double>(node->nb_visit));
                return avg_rejects - exploration;
        };
        Node* current = root;

        while (!current->children.empty()) {
                if (std::ranges::all_of(
                        current->children, [](const auto& node) { return node->leaf; })) {
                        current->leaf = true;
                        current = current->parent;
                }
                else {
                        // For each children select the one with the bigger score
                        auto best = std::ranges::min_element(
                            current->children, [&score](const auto& first, const auto& second) {
                                    if (first->leaf) { return false; }
                                    if (second->leaf) { return true; }
                                    return score(first) < score(second);
                            });

                        current = best->get();
                }
        }
        return current;
}

auto get_random_alloc() -> std::size_t
{
        static thread_local unsigned long long x = 0x9E3779B97F4A7C15ull;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        const unsigned long long r = x * 2685821657736338717ull;
        return static_cast<std::size_t>(r & 1ull);
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

void destroy_tree_iter(std::unique_ptr<Node> root)
{
        std::stack<Node*> stk;
        stk.push(root.release()); // Release ownership, now raw

        while (!stk.empty()) {
                Node* node = stk.top();
                stk.pop();
                for (auto& c : node->children)
                        stk.push(
                            c.release()); // Release children (get raw pointer, not deleted yet)
                delete node;
        }
}

auto run_monte_carlo(
    const std::size_t thread_id,
    const int MAX_SIM,
    const auto& prepattern,
    auto config,
    auto taskset,
    auto plat,
    std::atomic<bool>& stop_flag) -> std::pair<std::size_t, std::size_t>

{
        auto tree = std::make_unique<Node>();
        Node* current_node = tree.get();

        double best = std::numeric_limits<double>::infinity();
        std::size_t best_reject = std::numeric_limits<std::size_t>::infinity();
        std::size_t nb_leaf_found = 0;

        for (int i = 1; i <= MAX_SIM; ++i) {
                if (stop_flag.load(std::memory_order_acquire)) { break; }
                current_node = selection(tree.get());

                add_nodes(current_node);
                const auto branch = get_random_alloc();
                current_node = current_node->children.at(branch).get();

                // auto pattern = prepattern + get_first_pattern(current_node);
                auto pattern = get_first_pattern(current_node);
                pattern.insert(pattern.begin(), prepattern.begin(), prepattern.end());
                auto [score, nb_alloc] = simulate(config, taskset, plat, pattern);

                if (nb_alloc <= pattern.size()) {
                        if (!current_node->leaf) { nb_leaf_found++; }
                        current_node->leaf = true;
                        const double ratio =
                            static_cast<double>(score) / static_cast<double>(nb_alloc);
                        if (ratio < best) {
                                best = ratio;
                                best_reject = score;
                                std::println("T{} new best = {}", thread_id, best);
                                if (best == 0) {
                                        stop_flag.store(true, std::memory_order_release);
                                        break;
                                }
                        }
                }
                backpropagate(current_node, score);
        }

        destroy_tree_iter(std::move(tree));

        std::println("finished from thread {}", thread_id);

        // std::println("best: {}", best);
        // treeToDot(&tree, "tree.dot");
        return {best_reject, nb_leaf_found};
}

auto main(const int argc, const char** argv) -> int
{
        using namespace std;

        const bool FREESCALING_ALLOWED{false};

        try {
                const auto config = parse_args(argc, argv);
                const auto taskset = protocols::scenario::read_file(config.scenario_file);
                const auto plat = protocols::hardware::read_file(config.platform_file);

                // Determine number of threads from available processors
                std::size_t nb_threads = static_cast<std::size_t>(std::max(1, omp_get_num_procs()));
                omp_set_num_threads(static_cast<int>(nb_threads));

                // Generate prepatterns to split the MCTS root into nb_threads subtrees.
                // We create binary patterns of length L = ceil(log2(nb_threads)) and
                // take the first nb_threads combinations.
                auto generate_prepatterns = [](std::size_t n) {
                        std::vector<std::vector<unsigned>> res;
                        if (n == 0) { return res; }
                        std::size_t L = 0;
                        while ((1ull << L) < n) {
                                ++L;
                        }
                        res.reserve(n);
                        for (std::size_t i = 0; i < n; ++i) {
                                std::vector<unsigned> pattern(L);
                                for (std::size_t b = 0; b < L; ++b) {
                                        pattern[L - 1 - b] = static_cast<unsigned>((i >> b) & 1u);
                                }
                                res.push_back(std::move(pattern));
                        }
                        return res;
                };

                auto prepatterns = generate_prepatterns(nb_threads);
                std::println(
                    "prepatterns={} ; omp_max_threads={} ; nb_procs={}",
                    prepatterns.size(),
                    omp_get_max_threads(),
                    omp_get_num_procs());

                const std::size_t MAX_SIM = 100'000'000;
                const std::size_t base_sim = MAX_SIM / nb_threads;
                const std::size_t remainder = MAX_SIM % nb_threads;
                std::vector<std::size_t> results(nb_threads);
                std::vector<std::size_t> leafs_found(nb_threads);
                std::atomic<bool> stop_flag{false};

                auto format_vector = [](const std::vector<std::size_t>& v) {
                        std::ostringstream oss;
                        oss << "[";
                        for (std::size_t i = 0; i < v.size(); ++i) {
                                if (i > 0) oss << ", ";
                                oss << v[i];
                        }
                        oss << "]";
                        return oss.str();
                };

                auto format_vector_unsigned = [](const std::vector<unsigned>& v) {
                        std::ostringstream oss;
                        oss << "[";
                        for (std::size_t i = 0; i < v.size(); ++i) {
                                if (i > 0) oss << ", ";
                                oss << v[i];
                        }
                        oss << "]";
                        return oss.str();
                };

                for (std::size_t i = 0; i < prepatterns.size(); ++i) {
                        std::println("{}", format_vector_unsigned(prepatterns.at(i)));
                }

#pragma omp parallel
                {
                        auto index = static_cast<std::size_t>(omp_get_thread_num());
                        const std::size_t sims_for_this_thread =
                            base_sim + (index < remainder ? 1 : 0);
                        std::println("sim for this thread : {}", sims_for_this_thread);
                        auto [best, leafs] = run_monte_carlo(
                            index,
                            static_cast<int>(sims_for_this_thread),
                            prepatterns.at(index),
                            config,
                            taskset,
                            plat,
                            stop_flag);
                        results.at(index) = best;
                        leafs_found.at(index) = leafs;
                }

                if (stop_flag.load(std::memory_order_acquire)) {
                        std::println("Early stop requested, exiting.");
                }
                std::println("{}", format_vector(results));
                const std::size_t best_result = *std::ranges::min_element(results);
                const std::size_t nb_leafs =
                    std::accumulate(leafs_found.begin(), leafs_found.end(), 0);
                std::println("best result = {}, nb leafs found = {}", best_result, nb_leafs);

                std::ofstream datafile("mcts-result.csv", std::ios::app);
                if (!datafile) { return 1; }

                datafile << config.scenario_file << ";optimal;" << best_result << std::endl;
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
