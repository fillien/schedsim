#include "scenario.hpp"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

auto scenario::to_json(const scenario::job& job) -> nlohmann::json
{
        return {{"arrival", job.arrival}, {"duration", job.duration}};
}

auto scenario::to_json(const scenario::task& task) -> nlohmann::json
{
        nlohmann::json json_task{
            {"id", task.id}, {"period", task.period}, {"utilization", task.utilization}};

        for (const auto& job : task.jobs) {
                json_task["jobs"].push_back(to_json(job));
        }

        return json_task;
}

auto scenario::to_json(const scenario::setting& setting) -> nlohmann::json
{
        nlohmann::json json_setting{{"cores", setting.nb_cores}, {"tasks", {}}};
        for (const auto& task : setting.tasks) {
                json_setting["tasks"].push_back(to_json(task));
        }

        return json_setting;
}

auto scenario::from_json_job(const nlohmann::json& json_job) -> scenario::job
{
        return scenario::job{
            .arrival = json_job["arrival"].get<double>(),
            .duration = json_job["duration"].get<double>()};
}

auto scenario::from_json_task(const nlohmann::json& json_task) -> scenario::task
{
        scenario::task parsed_task{
            .id = json_task.at("id").get<uint16_t>(),
            .utilization = json_task.at("utilization").get<double>(),
            .period = json_task.at("period").get<double>(),
            .jobs = {}};

        for (const auto& json_job : json_task.at("jobs")) {
                parsed_task.jobs.push_back(from_json_job(json_job));
        }

        return parsed_task;
}

auto scenario::from_json_setting(const nlohmann::json& json_setting) -> scenario::setting
{
        scenario::setting setting{
            .nb_cores = json_setting.at("cores").get<uint16_t>(), .tasks = {}};
        for (const auto& json_task : json_setting.at("tasks")) {
                setting.tasks.push_back(from_json_task(json_task));
        }
        return setting;
}

void scenario::write_file(const std::filesystem::path& file, const scenario::setting& tasks)
{
        std::ofstream out(file);
        if (!out) { throw std::runtime_error("Unable to open file: " + file.string()); }

        out << to_json(tasks).dump();
}

auto scenario::read_file(const std::filesystem::path& file) -> scenario::setting
{
        std::ifstream input_file(file);
        if (!input_file) { throw std::runtime_error("Failed to open file: " + file.string()); }

        std::string input(
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
