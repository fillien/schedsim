#include "json.hpp"
#include "rtsched.hpp"
#include "textual.hpp"
#include "trace.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <vector>

/// @TODO Write the helper
void print_helper(std::ostream& out) { out << "TODO Write the helper" << std::endl; }

auto main(int argc, char* argv[]) -> int
{
        using namespace std::filesystem;

        const path DEFAULT_INPUT_FILE{"out.json"};

        path input_filepath{DEFAULT_INPUT_FILE};
        path output_filepath{};
        bool has_defined_output_file{false};

        const std::vector<std::string_view> args(argv + 1, argv + argc);
        std::vector<std::pair<double, traces::trace>> input_traces{};

        if (args.empty()) {
                throw std::runtime_error("No input traces file");
        }

        for (auto arg = std::begin(args); arg != std::end(args); ++arg) {
                if (*arg == "-o" || *arg == "--output") {
                        if (has_defined_output_file) {
                                throw std::runtime_error("Already defined a output file");
                        }
                        arg = std::next(arg);
                        output_filepath = *arg;
                        has_defined_output_file = true;
                }
                else if (*arg == "-h" || *arg == "--help") {
                        print_helper(std::cout);
                        return EXIT_SUCCESS;
                }
                else {
                        input_filepath = *arg;
                        if (!exists(input_filepath)) {
                                throw std::runtime_error(
                                    std::string(input_filepath) + ": No such file");
                        }
                        if (!input_filepath.has_extension() ||
                            input_filepath.extension() != ".json") {
                                throw std::runtime_error(
                                    std::string(input_filepath) + ": Not a JSON trace file");
                        }
                        break;
                }
        }

        std::string input_json{};

        {
                std::ifstream input_file{input_filepath};
                input_file >> input_json;
        }

        inputs::json::parse(input_json, input_traces);
        outputs::textual::print(std::cout, input_traces);

        if (has_defined_output_file) {
                std::ofstream output_file(output_filepath);
                outputs::rtsched::print(output_file, input_traces);
        }

        return EXIT_SUCCESS;
}
