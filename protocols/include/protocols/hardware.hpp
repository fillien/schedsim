#ifndef HARDWARE_HPP
#define HARDWARE_HPP

#include <cstddef>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <rapidjson/rapidjson.h>
#include <vector>

namespace protocols::hardware {
struct hardware {
        std::size_t nb_procs;
        std::vector<double> frequencies;
        double effective_freq;
        std::vector<double> power_model;
};

auto to_json(const hardware& plat) -> nlohmann::json;
auto from_json_hardware(const nlohmann::json& json_hardware) -> hardware;
void write_file(const std::filesystem::path& file, const hardware& plat);
auto read_file(const std::filesystem::path& file) -> hardware;
} // namespace protocols::hardware

#endif
