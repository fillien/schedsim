#ifndef SCENARIO_HPP
#define SCENARIO_HPP

#include "nlohmann/json_fwd.hpp"
#include <cstdint>
#include <filesystem>
#include <vector>

namespace scenario {
struct job {
        double arrival;
        double duration;
};

struct task {
        uint16_t id;
        double utilization;    // Utilization factor
        double period;         // Period of the task
        std::vector<job> jobs; // Jobs of the task
};

struct setting {
        uint16_t nb_cores;
        std::vector<task> tasks;
};

auto to_json(const setting& tasks) -> nlohmann::json;
auto from_json(const nlohmann::json& json_task) -> task;

void write_file(const std::filesystem::path& file, const setting& tasks);
auto read_file(std::filesystem::path& file) -> setting;
} // namespace scenario

#endif
