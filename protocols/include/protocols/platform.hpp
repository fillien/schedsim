#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include <cstddef>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <vector>

namespace protocols::platform {
struct platform {
        std::size_t nb_procs;
        std::vector<double> frequencies;
};

auto to_json(const platform& plat) -> nlohmann::json;
auto from_json_platform(const nlohmann::json& json_platform) -> platform;
void write_file(const std::filesystem::path& file, const platform& plat);
auto read_file(const std::filesystem::path& file) -> platform;
} // namespace protocols::platform

#endif
