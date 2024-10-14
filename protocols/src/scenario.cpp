#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <protocols/scenario.hpp>
#include <string>
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace protocols::scenario {
auto to_json(const job& job) -> nlohmann::json
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        return {{"arrival", job.arrival}, {"duration", job.duration}};
}

auto to_json(const task& task) -> nlohmann::json
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        nlohmann::json json_task{
            {"id", task.id}, {"period", task.period}, {"utilization", task.utilization}};

        for (const auto& job : task.jobs) {
                json_task["jobs"].push_back(to_json(job));
        }

        return json_task;
}

auto to_json(const setting& setting) -> nlohmann::json
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        nlohmann::json tasks{};
        for (const auto& task : setting.tasks) {
                tasks.push_back(to_json(task));
        }

        return nlohmann::json{{"tasks", tasks}};
}

auto from_json_job(const nlohmann::json& json_job) -> job
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        return job{
            .arrival = json_job["arrival"].get<double>(),
            .duration = json_job["duration"].get<double>()};
}

auto from_json_task(const nlohmann::json& json_task) -> task
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        task parsed_task{
            .id = json_task.at("id").get<std::size_t>(),
            .utilization = json_task.at("utilization").get<double>(),
            .period = json_task.at("period").get<double>(),
            .jobs = {}};

        for (const auto& json_job : json_task.at("jobs")) {
                parsed_task.jobs.push_back(from_json_job(json_job));
        }

        return parsed_task;
}

auto from_json_setting(const nlohmann::json& json_setting) -> setting
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::vector<task> tasks;
        for (const auto& json_task : json_setting.at("tasks")) {
                tasks.push_back(from_json_task(json_task));
        }
        return scenario::setting{tasks};
}

void write_file(const std::filesystem::path& file, const setting& tasks)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::ofstream out(file);
        if (!out) { throw std::runtime_error("Unable to open file: " + file.string()); }

        out << to_json(tasks).dump();
}

auto read_file(const std::filesystem::path& file) -> setting
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::ifstream input_file(file);
        if (!input_file) { throw std::runtime_error("Failed to open file: " + file.string()); }

        const std::string input(
            (std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
        input_file.close();

        try {
                auto json_input = nlohmann::json::parse(input);
                return from_json_setting(json_input);
        }
        catch (const nlohmann::json::parse_error& e) {
                throw std::runtime_error(
                    "JSON parsing error in file " + file.string() + ": " + e.what());
        }
}
} // namespace protocols::scenario
