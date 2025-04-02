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
                [&writer](const JobArrival& tra) {
                        writer.Key("type");
                        writer.String("job_arrival");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("duration");
                        writer.Double(tra.duration);
                        writer.Key("deadline");
                        writer.Double(tra.deadline);
                },
                [&writer](const JobFinished& tra) {
                        writer.Key("type");
                        writer.String("job_finished");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const ProcIdled& tra) {
                        writer.Key("type");
                        writer.String("proc_idled");
                        writer.Key("cpu");
                        writer.Uint(tra.proc_id);
                        writer.Key("cluster_id");
                        writer.Uint64(tra.cluster_id);
                },
                [&writer](const ProcActivated& tra) {
                        writer.Key("type");
                        writer.String("proc_activated");
                        writer.Key("cpu");
                        writer.Uint(tra.proc_id);
                        writer.Key("cluster_id");
                        writer.Uint64(tra.cluster_id);
                },
                [&writer](const ProcSleep& tra) {
                        writer.Key("type");
                        writer.String("proc_sleep");
                        writer.Key("cpu");
                        writer.Uint(tra.proc_id);
                        writer.Key("cluster_id");
                        writer.Uint64(tra.cluster_id);
                },
                [&writer](const ProcChange& tra) {
                        writer.Key("type");
                        writer.String("proc_change");
                        writer.Key("cpu");
                        writer.Uint(tra.proc_id);
                        writer.Key("cluster_id");
                        writer.Uint64(tra.cluster_id);
                },
                [&writer]([[maybe_unused]] const Resched& tra) {
                        writer.Key("type");
                        writer.String("resched");
                },
                [&writer](const ServNonCont& tra) {
                        writer.Key("type");
                        writer.String("serv_non_cont");
                        writer.Key("sid");
                        writer.Uint(tra.sched_id);
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const ServBudgetExhausted& tra) {
                        writer.Key("type");
                        writer.String("serv_budget_exhausted");
                        writer.Key("sid");
                        writer.Uint(tra.sched_id);
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const ServBudgetReplenished& tra) {
                        writer.Key("type");
                        writer.String("serv_budget_replenished");
                        writer.Key("sid");
                        writer.Uint(tra.sched_id);
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("budget");
                        writer.Double(tra.budget);
                },
                [&writer](const ServInactive& tra) {
                        writer.Key("type");
                        writer.String("serv_inactive");
                        writer.Key("sid");
                        writer.Uint(tra.sched_id);
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("utilization");
                        writer.Double(tra.utilization);
                },
                [&writer](const ServPostpone& tra) {
                        writer.Key("type");
                        writer.String("serv_postpone");
                        writer.Key("sid");
                        writer.Uint(tra.sched_id);
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("deadline");
                        writer.Double(tra.deadline);
                },
                [&writer](const ServReady& tra) {
                        writer.Key("type");
                        writer.String("serv_ready");
                        writer.Key("sid");
                        writer.Uint(tra.sched_id);
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("deadline");
                        writer.Double(tra.deadline);
                        writer.Key("utilization");
                        writer.Double(tra.utilization);
                },
                [&writer](const ServRunning& tra) {
                        writer.Key("type");
                        writer.String("serv_running");
                        writer.Key("sid");
                        writer.Uint(tra.sched_id);
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const TaskPreempted& tra) {
                        writer.Key("type");
                        writer.String("task_preempted");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const TaskScheduled& tra) {
                        writer.Key("type");
                        writer.String("task_scheduled");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("cpu");
                        writer.Uint(tra.proc_id);
                },
                [&writer](const TaskRejected& tra) {
                        writer.Key("type");
                        writer.String("task_rejected");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                },
                [&writer](const VirtualTimeUpdate& tra) {
                        writer.Key("type");
                        writer.String("virtual_time_update");
                        writer.Key("tid");
                        writer.Uint(tra.task_id);
                        writer.Key("virtual_time");
                        writer.Double(tra.virtual_time);
                },
                [&writer](const FrequencyUpdate& tra) {
                        writer.Key("type");
                        writer.String("frequency_update");
                        writer.Key("cluster_id");
                        writer.Uint64(tra.cluster_id);
                        writer.Key("frequency");
                        writer.Double(tra.frequency);
                },
                [&writer]([[maybe_unused]] const SimFinished& tra) {
                        writer.Key("type");
                        writer.String("sim_finished");
                },
                [&writer](const TaskPlaced& tra) {
                        writer.Key("type");
                        writer.String("task_placed");
                        writer.Key("tid");
                        writer.Uint64(tra.task_id);
                        writer.Key("cluster_id");
                        writer.Uint64(tra.cluster_id);
                },
                [&writer](const MigrationCluster& tra) {
                        writer.Key("type");
                        writer.String("migration_cluster");
                        writer.Key("tid");
                        writer.Uint64(tra.task_id);
                        writer.Key("cluster_id");
                        writer.Uint64(tra.cluster_id);
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
             [](const rapidjson::Value& log [[maybe_unused]]) -> trace { return SimFinished{}; }},
            {"resched",
             [](const rapidjson::Value& log [[maybe_unused]]) -> trace { return Resched{}; }},
            {"job_arrival",
             [](const rapidjson::Value& log) -> trace {
                     return JobArrival{
                         .task_id = log["tid"].GetUint64(),
                         .duration = log["duration"].GetDouble(),
                         .deadline = log["deadline"].GetDouble()};
             }},
            {"job_finished",
             [](const rapidjson::Value& log) -> trace {
                     return JobFinished{log["tid"].GetUint64()};
             }},
            {"proc_activated",
             [](const rapidjson::Value& log) -> trace {
                     return ProcActivated{
                         .proc_id = log["cpu"].GetUint64(),
                         .cluster_id = log["cluster_id"].GetUint64()};
             }},
            {"proc_sleep",
             [](const rapidjson::Value& log) -> trace {
                     return ProcSleep{
                         .proc_id = log["cpu"].GetUint64(),
                         .cluster_id = log["cluster_id"].GetUint64()};
             }},
            {"proc_idled",
             [](const rapidjson::Value& log) -> trace {
                     return ProcIdled{
                         .proc_id = log["cpu"].GetUint64(),
                         .cluster_id = log["cluster_id"].GetUint64()};
             }},
            {"proc_change",
             [](const rapidjson::Value& log) -> trace {
                     return ProcChange{
                         .proc_id = log["cpu"].GetUint64(),
                         .cluster_id = log["cluster_id"].GetUint64()};
             }},
            {"serv_budget_replenished",
             [](const rapidjson::Value& log) -> trace {
                     return ServBudgetReplenished{
                         .sched_id = log["sid"].GetUint(),
                         .task_id = log["tid"].GetUint64(),
                         .budget = log["budget"].GetDouble()};
             }},
            {"serv_inactive",
             [](const rapidjson::Value& log) -> trace {
                     return ServInactive{
                         .sched_id = log["sid"].GetUint(),
                         .task_id = log["tid"].GetUint64(),
                         .utilization = log["utilization"].GetDouble()};
             }},
            {"serv_running",
             [](const rapidjson::Value& log) -> trace {
                     return ServRunning{
                         .sched_id = log["sid"].GetUint(), .task_id = log["tid"].GetUint64()};
             }},
            {"serv_budget_exhausted",
             [](const rapidjson::Value& log) -> trace {
                     return ServBudgetExhausted{
                         .sched_id = log["sid"].GetUint(), .task_id = log["tid"].GetUint64()};
             }},
            {"serv_non_cont",
             [](const rapidjson::Value& log) -> trace {
                     return ServNonCont{
                         .sched_id = log["sid"].GetUint(), .task_id = log["tid"].GetUint64()};
             }},
            {"serv_postpone",
             [](const rapidjson::Value& log) -> trace {
                     return ServPostpone{
                         .sched_id = log["sid"].GetUint(),
                         .task_id = log["tid"].GetUint64(),
                         .deadline = log["deadline"].GetDouble()};
             }},
            {"serv_ready",
             [](const rapidjson::Value& log) -> trace {
                     return ServReady{
                         .sched_id = log["sid"].GetUint(),
                         .task_id = log["tid"].GetUint64(),
                         .deadline = log["deadline"].GetDouble(),
                         .utilization = log["utilization"].GetDouble()};
             }},
            {"task_preempted",
             [](const rapidjson::Value& log) -> trace {
                     return TaskPreempted{log["tid"].GetUint64()};
             }},
            {"task_scheduled",
             [](const rapidjson::Value& log) -> trace {
                     return TaskScheduled{
                         .task_id = log["tid"].GetUint64(), .proc_id = log["cpu"].GetUint64()};
             }},
            {"task_rejected",
             [](const rapidjson::Value& log) -> trace {
                     return TaskRejected{log["tid"].GetUint64()};
             }},
            {"virtual_time_update",
             [](const rapidjson::Value& log) -> trace {
                     return VirtualTimeUpdate{
                         .task_id = log["tid"].GetUint64(),
                         .virtual_time = log["virtual_time"].GetDouble()};
             }},
            {"frequency_update",
             [](const rapidjson::Value& log) -> trace {
                     return FrequencyUpdate{
                         .cluster_id = log["cluster_id"].GetUint64(),
                         .frequency = log["frequency"].GetDouble()};
             }},
            {"task_placed",
             [](const rapidjson::Value& log) -> trace {
                     return TaskPlaced{
                         .task_id = log["tid"].GetUint64(),
                         .cluster_id = log["cluster_id"].GetUint64()};
             }},
            {"migration_cluster",
             [](const rapidjson::Value& log) -> trace {
                     return MigrationCluster{
                         .task_id = log["tid"].GetUint64(),
                         .cluster_id = log["cluster_id"].GetUint64()};
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
