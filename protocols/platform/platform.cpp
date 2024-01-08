#include "platform.hpp"
#include "nlohmann/json.hpp"

#include <fstream>
#include <vector>

namespace protocols {
auto to_json(const platform& plat) -> nlohmann::json
{
        return {{"procs", plat.nb_procs}, {"frequencies", plat.frequencies}};
}

auto from_json_platform(const nlohmann::json& json_platform) -> platform
{
        return platform{
            .nb_procs = json_platform.at("procs").get<std::size_t>(),
            .frequencies = json_platform.at("frequencies").get<std::vector<double>>()};
}

void write_file(const std::filesystem::path& file, const platform& plat)
{
        std::ofstream out(file);
        if (!out) { throw std::runtime_error("Unable to open file: " + file.string()); }

        out << to_json(plat).dump();
}

auto read_file(const std::filesystem::path& file) -> platform
{
        std::ifstream input_file(file);
        if (!input_file) { throw std::runtime_error("Failed to open file: " + file.string()); }

        std::string input(
            (std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
        input_file.close();

        try {
                auto json_input = nlohmann::json::parse(input);
                return from_json_platform(json_input);
        }
        catch (const nlohmann::json::parse_error& e) {
                throw std::runtime_error(
                    "JSON parsing error in file " + file.string() + ": " + e.what());
        }
}
} // namespace protocols
