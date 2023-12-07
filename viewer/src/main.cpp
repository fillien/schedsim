#include "energy.hpp"
#include "rtsched.hpp"
#include "textual.hpp"
#include "traces.hpp"

#include "cxxopts.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

auto main(int argc, char* argv[]) -> int
{
        using namespace std::filesystem;

        const path DEFAULT_INPUT_FILE{"out.json"};

        cxxopts::Options options("viewer", "Analyze simulation trace and produce stats and plots");
        options.add_options()("h,help", "Helper")("p,print", "Print trace logs")(
            "e,energy", "Plot power & cumulative energy comsumption")(
            "r,rtsched", "Generate RTSched latex file", cxxopts::value<std::string>())(
            "traces", "Traces from simulator", cxxopts::value<std::string>());

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
                auto parsed = traces::read_log_file(input_filepath);

                if (cli.count("print")) { outputs::textual::print(std::cout, parsed); }

                if (cli.count("energy")) { outputs::energy::plot(parsed); }

                if (cli.count("rtsched")) {
                        std::filesystem::path output_file(cli["rtsched"].as<std::string>());
                        std::ofstream fd(output_file);
                        outputs::rtsched::print(fd, parsed);
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
