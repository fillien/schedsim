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

auto to_json(const job& job) -> nlohmann::json;
auto to_json(const task& task) -> nlohmann::json;
auto to_json(const setting& setting) -> nlohmann::json;
auto from_json_job(const nlohmann::json& json_job) -> job;
auto from_json_task(const nlohmann::json& json_task) -> task;
auto from_json_setting(const nlohmann::json& json_setting) -> setting;

void write_file(const std::filesystem::path& file, const setting& tasks);
auto read_file(const std::filesystem::path& file) -> setting;
} // namespace scenario

#endif
