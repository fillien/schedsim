#include <analyzers/deadline_misses.hpp>
#include <analyzers/energy.hpp>
#include <analyzers/frequency.hpp>
#include <analyzers/gantt/gantt.hpp>
#include <analyzers/gantt/rtsched.hpp>
#include <analyzers/gantt/svg.hpp>
#include <analyzers/stats.hpp>
#include <analyzers/textual.hpp>
#include <protocols/hardware.hpp>
#include <protocols/traces.hpp>

#include <any>
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

auto is_args_ask_table_result(const cxxopts::ParseResult& cli) -> bool
{
        return (
            cli.count("duration") || cli.count("preemptions") || cli.count("contextswitch") ||
            cli.count("rejected") || cli.count("waiting") || cli.count("arrivals") ||
            cli.count("cmigration") || cli.count("dpm-request") || cli.count("freq-request") ||
            cli.count("deadlines-rates") || cli.count("deadlines-counts") ||
            cli.count("transitions"));
}

auto is_args_ask_graph_result(const cxxopts::ParseResult& cli) -> bool
{
        return (
            cli.count("rtsched") || cli.count("frequency") || cli.count("cores") ||
            cli.count("energy") || cli.count("config") || cli.count("svg") || cli.count("html") ||
            cli.count("procmode") || cli.count("util"));
}

auto any_to_string(const std::any& a) -> std::string
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (a.type() == typeid(double)) { return std::to_string(std::any_cast<double>(a)); }
        if (a.type() == typeid(std::size_t)) {
                return std::to_string(std::any_cast<std::size_t>(a));
        }
        if (a.type() == typeid(std::string)) { return std::any_cast<std::string>(a); }
        return "Unknown type";
}

void print_table(const auto& table, const bool index)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (index) {
                for (auto it = table.begin(); it != table.end(); ++it) {
                        std::cout << it->first;
                        if (std::next(it) != table.end()) { std::cout << ';'; }
                }
                std::cout << '\n';
        }
        std::size_t size = table.begin()->second.size();
        for (std::size_t i = 0; i < size; ++i) {
                for (auto it = table.begin(); it != table.end(); ++it) {
                        std::cout << any_to_string(it->second.at(i));
                        if (std::next(it) != table.end()) { std::cout << ';'; }
                }
                std::cout << '\n';
        }
}

void handle_plots(const cxxopts::ParseResult& cli, const auto& parsed, const auto& hw)
{
        using namespace outputs::gantt;
        using namespace outputs::frequency;
        using namespace outputs::energy;

        if (cli.count("frequency")) {
                print_table(track_frequency_changes(parsed), cli.count("index"));
        }

        if (cli.count("cores")) { print_table(track_cores_changes(parsed), cli.count("index")); }

        if (cli.count("config")) { print_table(track_config_changes(parsed), cli.count("index")); }

        if (cli.count("energy")) {
                print_table(compute_energy_consumption(parsed, hw), cli.count("index"));
        }
        if (cli.count("util")) {
                print_table(outputs::stats::count_cores_utilization(parsed), cli.count("util"));
        }
        if (cli.count("rtsched")) {
                Gantt chart{generate_gantt(parsed, hw)};
                std::filesystem::path output_file(cli["rtsched"].as<std::string>());
                std::ofstream fd(output_file);
                fd << rtsched::draw(chart);
        }
        if (cli.count("procmode")) {
                Gantt chart{generate_proc_mode(parsed, hw)};
                std::cout << svg::draw(chart);
        }

        if (cli.count("svg")) {
                const Gantt gen_gantt = generate_gantt(parsed, hw);
                std::cout << svg::draw(gen_gantt);
        }

        if (cli.count("html")) {
                const Gantt gen_gantt = generate_gantt(parsed, hw);
                std::cout << html::draw(gen_gantt);
        }
}

void handle_table_args(
    const cxxopts::ParseResult& cli,
    const auto& hw [[maybe_unused]],
    std::map<std::string, std::vector<std::any>>& table,
    const std::string& file)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        using namespace outputs::stats;

        table["file"].push_back(file);
        auto parsed = protocols::traces::read_log_file(file);

        if (cli.count("preemptions")) {
                table["preemptions"].push_back(count_nb_preemption(parsed));
        }
        if (cli.count("contextswitch")) {
                table["contextswitch"].push_back(count_nb_contextswitch(parsed));
        }
        if (cli.count("rejected")) { table["rejected"].push_back(count_rejected(parsed)); }
        if (cli.count("cmigration")) {
                table["cmigration"].push_back(count_cluster_migration(parsed));
        }
        if (cli.count("arrivals")) { table["arrivals"].push_back(count_arrivals(parsed)); }
        if (cli.count("transitions")) {
                table["transitions"].push_back(count_possible_transition(parsed));
        }
        if (cli.count("waiting")) {
                table["waiting"].push_back(count_average_waiting_time(parsed));
        }
        if (cli.count("duration")) { table["duration"].push_back(count_duration(parsed)); }
        if (cli.count("dpm-request")) {
                table["dpm-request"].push_back(count_core_state_request(parsed));
        }
        if (cli.count("freq-request")) {
                table["freq-request"].push_back(count_frequency_request(parsed));
        }
        if (cli.count("deadlines-counts")) {
                auto deadlines = detect_deadline_misses(parsed);
                if (cli.count("deadlines-counts")) {
                        table["deadlines-counts"].push_back(count_deadline_missed(deadlines));
                }
        }
}

void handle_directory(
    const cxxopts::ParseResult& cli, const std::filesystem::path& dir, const auto& hardware)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace fs = std::filesystem;

        std::map<std::string, std::vector<std::any>> table;
        for (const auto& entry : fs::directory_iterator(dir)) {
                if (!fs::is_regular_file(entry.status())) {
                        throw std::runtime_error(entry.path().string() + " is not a file");
                }
                handle_table_args(cli, hardware, table, entry.path().string());
        }
        print_table(table, cli.count("index"));
}

void handle_outputs(const cxxopts::ParseResult& cli, const auto& parsed, const auto& hw)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace fs = std::filesystem;
        fs::path file_path = cli["infile"].as<std::string>();
        if (is_args_ask_table_result(cli)) {
                if (!fs::is_regular_file(file_path)) {
                        throw std::runtime_error(file_path.string() + " is not a file");
                }

                std::map<std::string, std::vector<std::any>> table;
                handle_table_args(cli, hw, table, file_path);
                print_table(table, cli.count("index"));
        }
        else if (is_args_ask_graph_result(cli)) {
                handle_plots(cli, parsed, hw);
        }
}

auto main(const int argc, const char** argv) -> int
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace fs = std::filesystem;

        cxxopts::Options options(
            "schedview",
            "Simulation Trace Analysis and Plot Generation Tool (For Post-Simulation Analysis of "
            "schedsim)");
        options.positional_help("infile");
        options.set_tab_expansion();
        // clang-format off
        options.add_options()
                ("h,help", "Show this help message.")
                ("p,print", "Print the trace logs.")
                ("d,directory", "Analyze all simulation traces within a directory.", cxxopts::value<std::string>())
                ("i,index", "Add column names to table data.")
                ("f,frequency", "Print frequency change events.")
                ("m,cores", "Print active core count changes.")
                ("c,config", "Print the timestamp start/stop at which the config stay the same.")
                ("change", "Print the total duration of change state on the scenario")
                ("r,rtsched", "Generate an RTSched LaTeX file.")
                ("procmode", "Generate RTSched LaTeX file with processor mode.")
                ("s,svg", "Generate a GANTT chart in SVG format.")
                ("html", "Generate a GANTT chart in HTML format.")
                ("util", "Print total utilization metrics.")
                ("e,energy", "Print the energy used by the platform during the simulation.")
                ("duration", "Print task set execution duration.")
                ("preemptions", "Print the number of preemptions.")
                ("contextswitch", "Print the number of context switches.")
                ("cmigration", "Print the number of cluster migration")
                ("transitions", "Print the number of transitions")
                ("rejected", "Print the number of tasks rejected by the admission test.")
                ("arrivals", "Print the number of job arrivals.")
                ("dpm-request", "Print the number of requests to change core C-state.")
                ("freq-request", "Print the number of requests to change frequency.")
                ("deadlines-rates", "Print the rate of missed deadlines.", cxxopts::value<std::size_t>()->implicit_value("0"))
                ("deadlines-counts", "Print the count of missed deadlines.", cxxopts::value<std::size_t>()->implicit_value("0"))
                ("platform", "Specify the hardware description file (default: platform.json).", cxxopts::value<std::string>()->default_value("platform.json"))
                ("infile", "Traces from simulator", cxxopts::value<std::string>());
        // clang-format on

        try {
                options.parse_positional({"infile"});
                const auto cli = options.parse(argc, argv);

                if (cli.count("help") || cli.arguments().empty()) {
                        std::cout << options.help() << std::endl;
                        exit(cli.arguments().empty() ? EXIT_FAILURE : EXIT_SUCCESS);
                }

                if ((is_args_ask_graph_result(cli) || is_args_ask_table_result(cli)) &&
                    cli.count("print")) {
                        throw std::runtime_error("cannot output graphs or table result, and logs");
                }
                else if (cli.count("print")) {
                        // Print and color the logs of the input file
                        fs::path input_filepath = cli["infile"].as<std::string>();
                        if (!fs::exists(input_filepath)) {
                                throw std::runtime_error(input_filepath.string() + " file missing");
                        }
                        auto parsed = protocols::traces::read_log_file(input_filepath);
                        outputs::textual::print(std::cout, parsed);
                }
                else {
                        fs::path platform_config = cli["platform"].as<std::string>();

                        if (!fs::exists(platform_config)) {
                                throw std::runtime_error(
                                    platform_config.string() + " file missing");
                        }

                        auto hardware = protocols::hardware::read_file(platform_config);

                        if (cli.count("directory")) {
                                // Handle the given directory
                                std::string dir_path = cli["directory"].as<std::string>();
                                if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
                                        throw std::runtime_error("directory does not exist "
                                                                 "or is not a directory");
                                }
                                handle_directory(cli, dir_path, hardware);
                        }
                        else {
                                // Handle only one file
                                fs::path file_path = cli["infile"].as<std::string>();
                                if (!fs::exists(file_path)) {
                                        throw std::runtime_error(
                                            file_path.string() + " file missing");
                                }
                                auto parsed = protocols::traces::read_log_file(file_path);
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
