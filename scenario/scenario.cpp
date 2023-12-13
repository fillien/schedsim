#include "scenario.hpp"
#include "nlohmann/detail/iterators/iter_impl.hpp"
#include "nlohmann/detail/json_ref.hpp"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

auto scenario::to_json(const scenario::setting& tasks) -> nlohmann::json
{
        using json = nlohmann::json;
        json out{{"cores", tasks.nb_cores}, {"tasks", {}}};

        for (const auto& t : tasks.tasks) {
                json json_task{{"id", t.id}, {"period", t.period}, {"utilization", t.utilization}};
                for (const auto& j : t.jobs) {
                        json json_job{{"arrival", j.arrival}, {"duration", j.duration}};
                        json_task["jobs"].push_back(json_job);
                }
                out["tasks"].push_back(json_task);
        }

        return out;
}

auto scenario::from_json(const nlohmann::json& json_task) -> scenario::task
{
        scenario::task parsed_task{
            .id = json_task.at("id").get<uint16_t>(),
            .utilization = json_task.at("utilization").get<double>(),
            .period = json_task.at("period").get<double>(),
            .jobs = {}};

        for (const auto& json_job : json_task.at("jobs")) {
                scenario::job parsed_job{
                    .arrival = json_job.at("arrival").get<double>(),
                    .duration = json_job.at("duration").get<double>()};
                parsed_task.jobs.push_back(parsed_job);
        }

        return parsed_task;
}

void scenario::write_file(const std::filesystem::path& file, const scenario::setting& tasks)
{
        std::ofstream out;
        out.open(file);
        while (!out.is_open()) {}
        out << to_json(tasks).dump();
        out.close();
}

auto scenario::read_file(std::filesystem::path& file) -> scenario::setting
{
        std::string input{};
        {
                std::ifstream input_file{file};
                if (input_file) {
                        std::ostringstream oss;
                        oss << input_file.rdbuf();
                        input = oss.str();
                }
        }

        auto json_input = nlohmann::json::parse(input);
        scenario::setting taskset{};
        taskset.nb_cores = json_input.at("cores");

        for (auto& json_task : json_input.at("tasks")) {
                taskset.tasks.push_back(from_json(json_task));
        }

        return taskset;
}
