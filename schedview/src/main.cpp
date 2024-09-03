
#include "deadline_misses.hpp"
#include "energy.hpp"
#include "frequency.hpp"
#include "gantt/gantt.hpp"
#include "gantt/rtsched.hpp"
#include "gantt/svg.hpp"
#include "protocols/hardware.hpp"
#include "stats.hpp"
#include "system_utilization.hpp"
#include "textual.hpp"
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

auto main(int argc, char* argv[]) -> int
{
        using namespace std::filesystem;

        const path DEFAULT_INPUT_FILE{"out.json"};

        cxxopts::Options options("viewer", "Analyze simulation trace and produce stats and plots");
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
                ("waiting", "Print average waiting time")
                ("deadlines-rates", "Print deadline missed rates", cxxopts::value<std::size_t>()->implicit_value("0"))
                ("deadlines-counts", "Print deadline missed counts", cxxopts::value<std::size_t>()->implicit_value("0"))
                ("platform", "Hardware description source file", cxxopts::value<std::string>()->default_value("platform.json"))
                ("traces", "Traces from simulator", cxxopts::value<std::string>());
        // clang-format on

        try {
                options.parse_positional({"traces"});
                const auto cli = options.parse(argc, argv);

                if (cli.arguments().empty()) {
                        std::cerr << options.help() << std::endl;
                        return EXIT_FAILURE;
                }

                if (cli.count("help")) {
                        std::cout << options.help() << std::endl;
                        return EXIT_SUCCESS;
                }

                if (!cli.count("traces")) {
                        std::cerr << "No input trace file" << std::endl;
                        return EXIT_FAILURE;
                }

                path input_filepath = cli["traces"].as<std::string>();
                if (!exists(input_filepath)) {
                        std::cerr << input_filepath << " no such file" << std::endl;
                        return EXIT_FAILURE;
                }
                auto parsed = protocols::traces::read_log_file(input_filepath);

                path platform_config = cli["platform"].as<std::string>();
                if (!exists(platform_config)) {
                        std::cerr << platform_config << " no such file" << std::endl;
                        return EXIT_FAILURE;
                }
                auto hardware = protocols::hardware::read_file(platform_config);

                if (cli.count("print")) { outputs::textual::print(std::cout, parsed); }

                if (cli.count("energy")) {
                        if (cli.count("cli")) {
                                // Only print the total energy consumption if CLI output wanted
                                outputs::energy::print_energy_consumption(parsed);
                        }
                        else {
                                outputs::energy::plot(parsed);
                        }
                }

                if (cli.count("frequency")) { outputs::frequency::print_frequency_changes(parsed); }

                if (cli.count("duration")) { outputs::stats::print_duration(parsed); }

                {
                        using namespace outputs::gantt;
                        if (cli.count("rtsched")) {
                                gantt chart{generate_gantt(parsed, hardware)};
                                std::filesystem::path output_file(cli["rtsched"].as<std::string>());
                                std::ofstream fd(output_file);
                                fd << rtsched::draw(chart);
                        }

                        if (cli.count("procmode")) {
                                gantt chart{generate_proc_mode(parsed, hardware)};
                                std::cout << svg::draw(chart);
                        }

                        if (cli.count("svg")) {
                                const gantt gen_gantt = generate_gantt(parsed, hardware);
                                std::cout << svg::draw(gen_gantt);
                        }

                        if (cli.count("html")) {
                                const gantt gen_gantt = generate_gantt(parsed, hardware);
                                std::cout << html::draw(gen_gantt);
                        }
                }

                if (cli.count("au")) { outputs::sys_util::print_active_utilization(parsed); }

                if (cli.count("preemptions")) { outputs::stats::print_nb_preemption(parsed); }

                if (cli.count("waiting")) { outputs::stats::print_average_waiting_time(parsed); }

                if (cli.count("deadlines-rates")) {
                        auto tid{cli["deadlines-rates"].as<std::size_t>()};
                        auto deadlines{outputs::stats::detect_deadline_misses(parsed)};
                        if (tid > 0) {
                                outputs::stats::print_task_deadline_missed_rate(deadlines, tid);
                        }
                        else {
                                outputs::stats::print_deadline_missed_rate(deadlines);
                        }
                }

                if (cli.count("deadlines-counts")) {
                        std::size_t tid{cli["deadlines-counts"].as<std::size_t>()};
                        auto deadlines{outputs::stats::detect_deadline_misses(parsed)};
                        std::cout << tid << std::endl;
                        if (tid > 0) {
                                outputs::stats::print_task_deadline_missed_count(deadlines, tid);
                        }
                        else {
                                outputs::stats::print_deadline_missed_count(deadlines);
                        }
                }
        }
        catch (cxxopts::exceptions::parsing& e) {
                std::cerr << "Error parsing casting option: " << e.what() << std::endl;
                return EXIT_FAILURE;
        }
        catch (std::bad_cast& e) {
                std::cerr << "Error parsing casting option: " << e.what() << std::endl;
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}
