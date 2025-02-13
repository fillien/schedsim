#ifndef SCENARIO_HPP
#define SCENARIO_HPP

#include <filesystem>
#include <rapidjson/document.h>
#include <vector>

namespace protocols::scenario {
struct Job {
        double arrival;
        double duration;
};

struct Task {
        uint64_t id;
        double utilization;    // Utilization factor
        double period;         // Period of the task
        std::vector<Job> jobs; // Jobs of the task
};

struct Setting {
        std::vector<Task> tasks;
};

void to_json(
    const Job& job, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& job_json);
void to_json(
    const Task& task, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& task_json);
void to_json(
    const Setting& setting,
    rapidjson::Document::AllocatorType& allocator,
    rapidjson::Value& setting_json);

auto from_json_job(const rapidjson::Value& json_job) -> Job;
auto from_json_task(const rapidjson::Value& json_task) -> Task;
auto from_json_setting(const rapidjson::Value& json_setting) -> Setting;

void write_file(const std::filesystem::path& file, const Setting& tasks);
auto read_file(const std::filesystem::path& file) -> Setting;
} // namespace protocols::scenario

#endif
