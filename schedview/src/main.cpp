
#include "deadline_misses.hpp"
#include "energy.hpp"
#include "frequency.hpp"
#include "gantt/gantt.hpp"
#include "gantt/rtsched.hpp"
#include "gantt/svg.hpp"
#include "protocols/hardware.hpp"
#include "stats.hpp"
#include "textual.hpp"
#include <iterator>
#include <protocols/traces.hpp>

#include <cstddef>
#include <cstdlib>
#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void handle_plots(const cxxopts::ParseResult& cli, const auto& parsed, const auto& hw)
{
        using namespace outputs::gantt;
        if (cli.count("frequency")) { outputs::frequency::print_frequency_changes(parsed); }

        if (cli.count("rtsched")) {
                gantt chart{generate_gantt(parsed, hw)};
                std::filesystem::path output_file(cli["rtsched"].as<std::string>());
                std::ofstream fd(output_file);
                fd << rtsched::draw(chart);
        }

        if (cli.count("procmode")) {
                gantt chart{generate_proc_mode(parsed, hw)};
                std::cout << svg::draw(chart);
        }

        if (cli.count("svg")) {
                const gantt gen_gantt = generate_gantt(parsed, hw);
                std::cout << svg::draw(gen_gantt);
        }

        if (cli.count("html")) {
                const gantt gen_gantt = generate_gantt(parsed, hw);
                std::cout << html::draw(gen_gantt);
        }
}

void handle_stats(const cxxopts::ParseResult& cli, const auto& parsed)
{
        using namespace outputs::stats;

        if (cli.count("preemptions")) { print_nb_preemption(parsed); }
        if (cli.count("contextswitch")) { print_nb_contextswitch(parsed); }
        if (cli.count("rejected")) { print_rejected(parsed); }
        if (cli.count("waiting")) { print_average_waiting_time(parsed); }
        if (cli.count("duration")) { print_duration(parsed); }
        if (cli.count("deadlines-rates")) {
                auto tid{cli["deadlines-rates"].as<std::size_t>()};
                auto deadlines{detect_deadline_misses(parsed)};
                if (tid > 0) { print_task_deadline_missed_rate(deadlines, tid); }
                else {
                        print_deadline_missed_rate(deadlines);
                }
        }
        if (cli.count("deadlines-counts")) {
                auto tid{cli["deadlines-counts"].as<std::size_t>()};
                auto deadlines{detect_deadline_misses(parsed)};
                if (tid > 0) { print_task_deadline_missed_count(deadlines, tid); }
                else {
                        print_deadline_missed_count(deadlines);
                }
        }
        if (cli.count("dpm-request")) { print_core_state_request_count(parsed); }
        if (cli.count("freq-request")) { print_frequency_request_count(parsed); }
        if (cli.count("energy")) { outputs::energy::print_energy_consumption(parsed); }
}

#include <set>

struct core_state_request_handler {
        std::set<std::size_t> active_cores;
        int cpt;

        void on_proc_activated(std::size_t proc_id)
        {
                if (!active_cores.contains(proc_id)) {
                        active_cores.insert(proc_id);
                        cpt++;
                }
        }

        void on_proc_idled(std::size_t proc_id)
        {
                if (!active_cores.contains(proc_id)) {
                        active_cores.insert(proc_id);
                        cpt++;
                }
        }

        void on_proc_sleep(std::size_t proc_id)
        {
                if (active_cores.contains(proc_id)) {
                        active_cores.erase(proc_id);
                        cpt++;
                }
        }
};

void handle_outputs(const cxxopts::ParseResult& cli, const auto& parsed, const auto& hw)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (cli.count("print")) { outputs::textual::print(std::cout, parsed); }
        handle_stats(cli, parsed);
        handle_plots(cli, parsed, hw);
        //
        // namespace tr = protocols::traces;
        //
        // const bool cse_enabled = cli.count("dpm-request");
        //
        // core_state_request_handler csr;
        //
        // for (const auto& [timestamp, evt] : parsed) {
        //         std::visit(
        //             overloaded{
        //                 [&](tr::proc_activated evt) {
        //                         csr.on_proc_activated(evt.proc_id);
        //                 },
        //                 [&](tr::proc_idled evt) {
        //                         csr.on_proc_idled(evt.proc_id);
        //                 },
        //                 [&](tr::proc_sleep evt) {
        //                         csr.on_proc_sleep(evt.proc_id);
        //                 },
        //                 [](auto) {}},
        //             evt);
        // }
}

auto main(const int argc, const char** argv) -> int
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace fs = std::filesystem;

        cxxopts::Options options("viewer", "Analyze simulation trace and produce stats and plots");
        options.positional_help("infile");
        options.set_tab_expansion();
        // clang-format off
        options.add_options()
                ("h,help", "Helper")
		        ("cli", "Print CLI friendly outputs")
                ("p,print", "Print trace logs")
                ("e,energy", "Plot power & cumulative energy comsumption")
                ("f,frequency", "Print frequency changes")
                ("r,rtsched", "Generate RTSched latex file")
                ("procmode", "Generate RTSched latex file")
                ("s,svg", "Generate GANTT chart in SVG file")
                ("html", "Generate GANTT chart in HTML file")
                ("au", "Print active utilization")
                ("duration", "Print taskset execution duration")
                ("preemptions", "Print number of preemption")
                ("contextswitch", "Print number of context switch")
                ("rejected", "Print number of rejected task")
                ("waiting", "Print average waiting time")
                ("dpm-request", "Print the number of requests to change the cores C-state")
                ("freq-request", "Print the number of requests to change the frequency")
                ("deadlines-rates", "Print deadline missed rates", cxxopts::value<std::size_t>()->implicit_value("0"))
                ("deadlines-counts", "Print deadline missed counts", cxxopts::value<std::size_t>()->implicit_value("0"))
                ("platform", "Hardware description source file", cxxopts::value<std::string>()->default_value("platform.json"))
                ("infile", "Traces from simulator", cxxopts::value<std::string>());
        // clang-format on

        try {
                options.parse_positional({"infile"});
                const auto cli = options.parse(argc, argv);

                if (cli.count("help") || cli.arguments().empty()) {
                        std::cout << options.help() << std::endl;
                        exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
                }

                fs::path input_filepath = cli["infile"].as<std::string>();
                fs::path platform_config = cli["platform"].as<std::string>();

                if (!fs::exists(input_filepath)) {
                        throw std::runtime_error(input_filepath.string() + " file missing");
                }

                if (!fs::exists(platform_config)) {
                        throw std::runtime_error(platform_config.string() + " file missing");
                }

                auto parsed = protocols::traces::read_log_file(input_filepath);
                auto hardware = protocols::hardware::read_file(platform_config);

                handle_outputs(cli, parsed, hardware);

                return EXIT_SUCCESS;
        }
        catch (const cxxopts::exceptions::parsing& e) {
                std::cerr << "Error parsing casting option: " << e.what() << std::endl;
        }
        catch (const std::bad_cast& e) {
                std::cerr << "Error parsing casting option: " << e.what() << std::endl;
        }
        catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
        }

        return EXIT_FAILURE;
}
