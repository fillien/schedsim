#ifndef SCENARIO_HPP
#define SCENARIO_HPP

#include <cstdint>
#include <filesystem>
#include <rapidjson/document.h>
#include <vector>

namespace protocols::scenario {
struct job {
        double arrival;
        double duration;
};

struct task {
        uint64_t id;
        double utilization;    // Utilization factor
        double period;         // Period of the task
        std::vector<job> jobs; // Jobs of the task
};

struct setting {
        std::vector<task> tasks;
};

void to_json(
    const job& job, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& job_json);
void to_json(
    const task& task, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& task_json);
void to_json(
    const setting& setting,
    rapidjson::Document::AllocatorType& allocator,
    rapidjson::Value& setting_json);

auto from_json_job(const rapidjson::Value& json_job) -> job;
auto from_json_task(const rapidjson::Value& json_task) -> task;
auto from_json_setting(const rapidjson::Value& json_setting) -> setting;

void write_file(const std::filesystem::path& file, const setting& tasks);
auto read_file(const std::filesystem::path& file) -> setting;
} // namespace protocols::scenario

#endif
