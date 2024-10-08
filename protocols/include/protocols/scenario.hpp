#ifndef SCENARIO_HPP
#define SCENARIO_HPP

#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <vector>

namespace protocols::scenario {
struct job {
        double arrival;
        double duration;
};

struct task {
        std::size_t id;
        double utilization;    // Utilization factor
        double period;         // Period of the task
        std::vector<job> jobs; // Jobs of the task
};

struct setting {
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
} // namespace protocols::scenario

#endif
