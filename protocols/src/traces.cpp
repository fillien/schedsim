#include <fstream>
#include <functional>
#include <map>
#include <protocols/traces.hpp>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
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
void to_json(const trace& log, rapidjson::Writer<rapidjson::OStreamWrapper>& writer)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::visit(
            overloaded{
                [&writer](const job_arrival& tra) {
                        writer.Key("type");
                        writer.String("job_arrival");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("duration");
                        writer.Double(tra.duration);
                        writer.Key("deadline");
                        writer.Double(tra.deadline);
                },
                [&writer](const job_finished& tra) {
                        writer.Key("type");
                        writer.String("job_finished");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const proc_idled& tra) {
                        writer.Key("type");
                        writer.String("proc_idled");
                        writer.Key("cpu");
                        writer.Uint(tra.proc_id);
                },
                [&writer](const proc_activated& tra) {
                        writer.Key("type");
                        writer.String("proc_activated");
                        writer.Key("cpu");
                        writer.Uint(tra.proc_id);
                },
                [&writer](const proc_sleep& tra) {
                        writer.Key("type");
                        writer.String("proc_sleep");
                        writer.Key("cpu");
                        writer.Uint(tra.proc_id);
                },
                [&writer]([[maybe_unused]] const resched& tra) {
                        writer.Key("type");
                        writer.String("resched");
                },
                [&writer](const serv_non_cont& tra) {
                        writer.Key("type");
                        writer.String("serv_non_cont");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const serv_budget_exhausted& tra) {
                        writer.Key("type");
                        writer.String("serv_budget_exhausted");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const serv_budget_replenished& tra) {
                        writer.Key("type");
                        writer.String("serv_budget_replenished");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("budget");
                        writer.Double(tra.budget);
                },
                [&writer](const serv_inactive& tra) {
                        writer.Key("type");
                        writer.String("serv_inactive");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("utilization");
                        writer.Double(tra.utilization);
                },
                [&writer](const serv_postpone& tra) {
                        writer.Key("type");
                        writer.String("serv_postpone");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("deadline");
                        writer.Double(tra.deadline);
                },
                [&writer](const serv_ready& tra) {
                        writer.Key("type");
                        writer.String("serv_ready");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("deadline");
                        writer.Double(tra.deadline);
                        writer.Key("utilization");
                        writer.Double(tra.utilization);
                },
                [&writer](const serv_running& tra) {
                        writer.Key("type");
                        writer.String("serv_running");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const task_preempted& tra) {
                        writer.Key("type");
                        writer.String("task_preempted");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const task_scheduled& tra) {
                        writer.Key("type");
                        writer.String("task_scheduled");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("cpu");
                        writer.Uint(tra.proc_id);
                },
                [&writer](const task_rejected& tra) {
                        writer.Key("type");
                        writer.String("task_rejected");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const virtual_time_update& tra) {
                        writer.Key("type");
                        writer.String("virtual_time_update");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("virtual_time");
                        writer.Double(tra.virtual_time);
                },
                [&writer](const frequency_update& tra) {
                        writer.Key("type");
                        writer.String("frequency_update");
                        writer.Key("frequency");
                        writer.Double(tra.frequency);
                },
                [&writer]([[maybe_unused]] const sim_finished& tra) {
                        writer.Key("type");
                        writer.String("sim_finished");
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

void write_log_file(const std::multimap<double, trace>& logs, const std::filesystem::path& file)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::ofstream ofs(file);
        if (!ofs.is_open()) { throw std::runtime_error("Failed to open file"); }

        rapidjson::OStreamWrapper osw(ofs);
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);

        writer.StartArray();
        for (const auto& [time, log] : logs) {
                writer.StartObject();
                writer.Key("time");
                writer.Double(time);
                to_json(log, writer);
                writer.EndObject();
        }
        writer.EndArray();

        ofs.close();
}

auto read_log_file(const std::filesystem::path& file) -> std::vector<std::pair<double, trace>>
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

        return parsed_traces;
}
} // namespace protocols::traces
