#include <fstream>
#include <iterator>
#include <map>
#include <nlohmann/json.hpp>
#include <ostream>
#include <protocols/traces.hpp>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <stdexcept>
#include <variant>
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace protocols::traces {
auto to_json(const trace& log) -> nlohmann::json
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        using json = nlohmann::json;
        return std::visit(
            overloaded{
                [](const job_arrival& tra) {
                        return json{
                            {"type", "job_arrival"},
                            {"tid", tra.task_id},
                            {"duration", tra.duration},
                            {"deadline", tra.deadline}};
                },
                [](const job_finished& tra) {
                        return json{{"type", "job_finished"}, {"tid", tra.task_id}};
                },
                [](const proc_idled& tra) {
                        return json{{"type", "proc_idled"}, {"cpu", tra.proc_id}};
                },
                [](const proc_activated& tra) {
                        return json{{"type", "proc_activated"}, {"cpu", tra.proc_id}};
                },
                [](const proc_sleep& tra) {
                        return json{{"type", "proc_sleep"}, {"cpu", tra.proc_id}};
                },
                []([[maybe_unused]] const resched& tra) { return json{{"type", "resched"}}; },
                [](const serv_non_cont& tra) {
                        return json{{"type", "serv_non_cont"}, {"tid", tra.task_id}};
                },
                [](const serv_budget_exhausted& tra) {
                        return json{{"type", "serv_budget_exhausted"}, {"tid", tra.task_id}};
                },
                [](const serv_budget_replenished& tra) {
                        return json{
                            {"type", "serv_budget_replenished"},
                            {"tid", tra.task_id},
                            {"budget", tra.budget}};
                },
                [](const serv_inactive& tra) {
                        return json{
                            {"type", "serv_inactive"},
                            {"tid", tra.task_id},
                            {"utilization", tra.utilization}};
                },
                [](const serv_postpone& tra) {
                        return json{
                            {"type", "serv_postpone"},
                            {"tid", tra.task_id},
                            {"deadline", tra.deadline}};
                },
                [](const serv_ready& tra) {
                        return json{
                            {"type", "serv_ready"},
                            {"tid", tra.task_id},
                            {"deadline", tra.deadline},
                            {"utilization", tra.utilization}};
                },
                [](const serv_running& tra) {
                        return json{{"type", "serv_running"}, {"tid", tra.task_id}};
                },
                [](const task_preempted& tra) {
                        return json{{"type", "task_preempted"}, {"tid", tra.task_id}};
                },
                [](const task_scheduled& tra) {
                        return json{
                            {"type", "task_scheduled"}, {"tid", tra.task_id}, {"cpu", tra.proc_id}};
                },
                [](const task_rejected& tra) {
                        return json{{"type", "task_rejected"}, {"tid", tra.task_id}};
                },
                [](const virtual_time_update& tra) {
                        return json{
                            {"type", "virtual_time_update"},
                            {"tid", tra.task_id},
                            {"virtual_time", tra.virtual_time}};
                },
                [](const frequency_update& tra) {
                        return json{{"type", "frequency_update"}, {"frequency", tra.frequency}};
                },
                []([[maybe_unused]] const sim_finished& tra) {
                        return json{{"type", "sim_finished"}};
                }},
            log);
}

auto from_json(const rapidjson::Value& log) -> trace
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif

        using trace_handler = std::function<trace(const rapidjson::Value&)>;

        // Map event types to their respective handlers
        const std::unordered_map<std::string_view, trace_handler> convert{
            {"sim_finished",
             [](const rapidjson::Value& log [[maybe_unused]]) -> trace { return sim_finished{}; }},
            {"resched",
             [](const rapidjson::Value& log [[maybe_unused]]) -> trace { return resched{}; }},
            {"job_arrival",
             [](const rapidjson::Value& log) -> trace {
                     return job_arrival{
                         log["tid"].GetUint64(),
                         log["duration"].GetDouble(),
                         log["deadline"].GetDouble()};
             }},
            {"job_finished",
             [](const rapidjson::Value& log) -> trace {
                     return job_finished{log["tid"].GetUint64()};
             }},
            {"proc_activated",
             [](const rapidjson::Value& log) -> trace {
                     return proc_activated{log["cpu"].GetUint64()};
             }},
            {"proc_sleep",
             [](const rapidjson::Value& log) -> trace {
                     return proc_sleep{log["cpu"].GetUint64()};
             }},
            {"proc_idled",
             [](const rapidjson::Value& log) -> trace {
                     return proc_idled{log["cpu"].GetUint64()};
             }},
            {"serv_budget_replenished",
             [](const rapidjson::Value& log) -> trace {
                     return serv_budget_replenished{
                         log["tid"].GetUint64(), log["budget"].GetDouble()};
             }},
            {"serv_inactive",
             [](const rapidjson::Value& log) -> trace {
                     return serv_inactive{log["tid"].GetUint64(), log["utilization"].GetDouble()};
             }},
            {"serv_running",
             [](const rapidjson::Value& log) -> trace {
                     return serv_running{log["tid"].GetUint64()};
             }},
            {"serv_budget_exhausted",
             [](const rapidjson::Value& log) -> trace {
                     return serv_budget_exhausted{log["tid"].GetUint64()};
             }},
            {"serv_non_cont",
             [](const rapidjson::Value& log) -> trace {
                     return serv_non_cont{log["tid"].GetUint64()};
             }},
            {"serv_postpone",
             [](const rapidjson::Value& log) -> trace {
                     return serv_postpone{log["tid"].GetUint64(), log["deadline"].GetDouble()};
             }},
            {"serv_ready",
             [](const rapidjson::Value& log) -> trace {
                     return serv_ready{
                         log["tid"].GetUint64(),
                         log["deadline"].GetDouble(),
                         log["utilization"].GetDouble()};
             }},
            {"task_preempted",
             [](const rapidjson::Value& log) -> trace {
                     return task_preempted{log["tid"].GetUint64()};
             }},
            {"task_scheduled",
             [](const rapidjson::Value& log) -> trace {
                     return task_scheduled{log["tid"].GetUint64(), log["cpu"].GetUint64()};
             }},
            {"task_rejected",
             [](const rapidjson::Value& log) -> trace {
                     return task_rejected{log["tid"].GetUint64()};
             }},
            {"virtual_time_update",
             [](const rapidjson::Value& log) -> trace {
                     return virtual_time_update{
                         log["tid"].GetUint64(), log["virtual_time"].GetDouble()};
             }},
            {"frequency_update",
             [](const rapidjson::Value& log) -> trace {
                     return frequency_update{log["frequency"].GetDouble()};
             }},
        };

        auto search = convert.find(log["type"].GetString());
        if (search != convert.end()) [[likely]] { return search->second(log); }
        throw std::out_of_range("Unsupported event type");
}

void write_log_file(const std::multimap<double, trace>& logs, std::filesystem::path& file)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::ofstream out;
        out.open(file);

        while (!out.is_open()) {}

        out << "[";
        for (auto itr = std::begin(logs); itr != std::end(logs); ++itr) {
                auto buffer = to_json((*itr).second);
                buffer.push_back({"time", (*itr).first});
                out << buffer.dump();
                if (itr != std::prev(logs.end())) { out << ","; }
        }
        out << "]";
        out.close();
}

auto read_log_file(std::filesystem::path& file) -> std::vector<std::pair<double, trace>>
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::ifstream input_file{file};

        if (!input_file.is_open()) { throw std::runtime_error("File could not be opened"); }

        rapidjson::IStreamWrapper isw(input_file);
        rapidjson::Document json_input;
        json_input.ParseStream(isw);

        if (json_input.HasParseError()) { throw std::runtime_error("Error parsing JSON"); }
        if (!json_input.IsArray()) { throw std::runtime_error("Expected JSON array"); }

        const auto& logs_array = json_input.GetArray();
        std::vector<std::pair<double, trace>> parsed_traces;
        parsed_traces.reserve(logs_array.Size());

        for (const auto& json_trace : logs_array) {
#ifdef TRACY_ENABLE
                ZoneScoped;
#endif
                if (!json_trace.HasMember("time") || !json_trace["time"].IsDouble()) [[unlikely]] {
                        throw std::runtime_error("Missing or invalid 'time' field");
                }
                parsed_traces.emplace_back(json_trace["time"].GetDouble(), from_json(json_trace));
        }

        std::sort(parsed_traces.begin(), parsed_traces.end(), [](const auto& a, const auto& b) {
                return a.first < b.first;
        });

        return parsed_traces;
}
} // namespace protocols::traces
