#include "deadline_misses.hpp"
#include "energy.hpp"
#include "frequency.hpp"
#include "gantt/gantt.hpp"
#include "gantt/rtsched.hpp"
#include "gantt/svg.hpp"
#include "protocols/hardware.hpp"
#include "stats.hpp"
#include "textual.hpp"
#include <any>
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

auto any_to_string(const std::any& a) -> std::string
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (a.type() == typeid(double)) { return std::to_string(std::any_cast<double>(a)); }
        else if (a.type() == typeid(std::size_t)) {
                return std::to_string(std::any_cast<std::size_t>(a));
        }
        else if (a.type() == typeid(std::string)) {
                return std::any_cast<std::string>(a);
        }
        std::cout << "Unknown type" << std::endl;
        return "";
}

void print_table(const auto& table)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        for (auto it = table.begin(); it != table.end(); ++it) {
                std::cout << it->first;
                if (std::next(it) != table.end()) { std::cout << ';'; }
        }
        std::cout << '\n';

        std::size_t size = table.begin()->second.size();
        for (std::size_t i = 0; i < size; ++i) {
                for (auto it = table.begin(); it != table.end(); ++it) {
                        std::cout << any_to_string(it->second.at(i));
                        if (std::next(it) != table.end()) { std::cout << ';'; }
                }
                std::cout << '\n';
        }
}

void handle_table_args(const cxxopts::ParseResult& cli, const auto& parsed, const auto& hw)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
}

void handle_outputs(const cxxopts::ParseResult& cli, const auto& parsed, const auto& hw)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        handle_table_args(cli, parsed, hw);
        // handle_plots(cli, parsed, hw);
}

auto is_args_ask_table_result(const cxxopts::ParseResult& cli) -> bool
{
        return (
            cli.count("energy") || cli.count("duration") || cli.count("preemptions") ||
            cli.count("contextswitch") || cli.count("rejected") || cli.count("waiting") ||
            cli.count("dpm-request") || cli.count("freq-request") || cli.count("deadlines-rates") ||
            cli.count("deadlines-counts"));
}

auto is_args_ask_graph_result(const cxxopts::ParseResult& cli) -> bool
{
        return (
            cli.count("rtsched") || cli.count("frequency") || cli.count("svg") ||
            cli.count("html") || cli.count("procmode") || cli.count("au"));
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
                ("p,print", "Print trace logs")
                ("d,directory", "Analyze a whole directory", cxxopts::value<std::string>())
                ("f,frequency", "Print frequency changes")
                ("r,rtsched", "Generate RTSched latex file")
                ("procmode", "Generate RTSched latex file")
                ("s,svg", "Generate GANTT chart in SVG file")
                ("html", "Generate GANTT chart in HTML file")
                ("au", "Print active utilization")
                ("e,energy", "Plot power & cumulative energy comsumption")
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
                fs::path platform_config = cli["platform"].as<std::string>();

                if (!fs::exists(platform_config)) {
                        throw std::runtime_error(platform_config.string() + " file missing");
                }

                if ((is_args_ask_graph_result(cli) || is_args_ask_table_result(cli)) &&
                    cli.count("print")) {
                        throw std::runtime_error("cannot output graphs or table result, and logs");
                }
                else if (cli.count("print")) {
                        fs::path input_filepath = cli["infile"].as<std::string>();
                        if (!fs::exists(input_filepath)) {
                                throw std::runtime_error(input_filepath.string() + " file missing");
                        }
                        auto parsed = protocols::traces::read_log_file(input_filepath);
                        outputs::textual::print(std::cout, parsed);
                }
                else {
                        auto hardware = protocols::hardware::read_file(platform_config);

                        if (cli.count("directory")) {
                                std::string dir_path = cli["directory"].as<std::string>();

                                // Get the list of files in the directory
                                if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                                        std::map<std::string, std::vector<std::any>> table;
                                        for (const auto& entry : fs::directory_iterator(dir_path)) {
                                                table["file"].push_back(entry.path().string());
                                                auto parsed =
                                                    protocols::traces::read_log_file(entry.path().string());
                                                if (fs::is_regular_file(entry.status())) {
                                                        using namespace outputs::stats;
                                                        using namespace outputs::energy;

                                                        if (cli.count("energy")) {
                                                                table["energy"].push_back(
                                                                    compute_energy_consumption(
                                                                        parsed, hardware));
                                                        }
                                                        if (cli.count("preemptions")) {
                                                                table["preemptions"].push_back(
                                                                    count_nb_preemption(parsed));
                                                        }
                                                        if (cli.count("contextswitch")) {
                                                                table["contextswitch"].push_back(
                                                                    count_nb_contextswitch(parsed));
                                                        }
                                                        if (cli.count("rejected")) {
                                                                table["rejected"].push_back(
                                                                    count_rejected(parsed));
                                                        }
                                                        if (cli.count("waiting")) {
                                                                table["waiting"].push_back(
                                                                    count_average_waiting_time(
                                                                        parsed));
                                                        }
                                                        if (cli.count("duration")) {
                                                                table["duration"].push_back(
                                                                    count_duration(parsed));
                                                        }
                                                        if (cli.count("dpm-request")) {
                                                                table["dpm-request"].push_back(
                                                                    count_core_state_request(
                                                                        parsed));
                                                        }
                                                        if (cli.count("freq-request")) {
                                                                table["freq-request"].push_back(
                                                                    count_frequency_request(
                                                                        parsed));
                                                        }
                                                        if (cli.count("deadlines-rates")) {
                                                                auto deadlines{
                                                                    detect_deadline_misses(parsed)};
                                                                table["deadlines-rates"].push_back(
                                                                    count_deadline_missed_rate(
                                                                        deadlines));
                                                        }
                                                        if (cli.count("deadlines-counts")) {
                                                                auto deadlines{
                                                                    detect_deadline_misses(parsed)};
                                                                table["deadlines-counts"].push_back(
                                                                    count_deadline_missed(
                                                                        deadlines));
                                                        }
                                                }
                                        }
                                        print_table(table);
                                }
                                else {
                                        throw std::runtime_error("directory does not exist "
                                                                 "or is not a directory");
                                }
                        }
                        else {
                                fs::path input_filepath = cli["infile"].as<std::string>();
                                if (!fs::exists(input_filepath)) {
                                        throw std::runtime_error(
                                            input_filepath.string() + " file missing");
                                }
                                auto parsed = protocols::traces::read_log_file(input_filepath);
                                handle_outputs(cli, parsed, hardware);
                        }
                }
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
