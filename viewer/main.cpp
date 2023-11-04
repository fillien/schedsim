#include "inputs/json.hpp"
#include "outputs/rtsched.hpp"
#include "outputs/textual.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

auto main() -> int
{
        using namespace std::filesystem;

        const path DEFAULT_INPUT_FILE{"out.json"};
        const path DEFAULT_OUTPUT_FILE{"mydessin.tex"};

        std::vector<std::pair<double, traces::trace>> input_traces;

        std::ifstream input_file{DEFAULT_INPUT_FILE};
        std::string input_json;
        input_file >> input_json;

        inputs::json::parse(input_json, input_traces);
        outputs::textual::print(std::cout, input_traces);

        std::ofstream output_file;
        output_file.open(DEFAULT_OUTPUT_FILE);
        outputs::rtsched::print(output_file, input_traces);
        output_file.close();

        return EXIT_SUCCESS;
}
