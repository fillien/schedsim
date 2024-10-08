#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <protocols/hardware.hpp>
#include <vector>

namespace protocols::hardware {
auto to_json(const hardware& plat) -> nlohmann::json
{
        return {
            {"procs", plat.nb_procs},
            {"frequencies", plat.frequencies},
            {"effective_freq", plat.effective_freq},
            {"power_model", plat.power_model}};
}

auto from_json_hardware(const nlohmann::json& json_hardware) -> hardware
{
        return hardware{
            .nb_procs = json_hardware.at("procs").get<std::size_t>(),
            .frequencies = json_hardware.at("frequencies").get<std::vector<double>>(),
            .effective_freq = json_hardware.at("effective_freq").get<double>(),
            .power_model = json_hardware.at("power_model").get<std::vector<double>>()};
}

void write_file(const std::filesystem::path& file, const hardware& plat)
{
        std::ofstream out(file);
        if (!out) { throw std::runtime_error("Unable to open file: " + file.string()); }

        out << to_json(plat).dump();
}

auto read_file(const std::filesystem::path& file) -> hardware
{
        std::ifstream input_file(file);
        if (!input_file) { throw std::runtime_error("Failed to open file: " + file.string()); }

        const std::string input(
            (std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
        input_file.close();

        try {
                auto json_input = nlohmann::json::parse(input);
                return from_json_hardware(json_input);
        }
        catch (const nlohmann::json::parse_error& e) {
                throw std::runtime_error(
                    "JSON parsing error in file " + file.string() + ": " + e.what());
        }
}
} // namespace protocols::hardware
