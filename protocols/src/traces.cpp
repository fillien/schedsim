#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <ostream>
#include <protocols/traces.hpp>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace protocols::traces {
auto to_json(const trace& log) -> nlohmann::json
{
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
                []([[maybe_unused]] const resched& tra) {
                        return json{{"type", "resched"}};
                },
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
                        return json{{"type", "serv_inactive"}, {"tid", tra.task_id}};
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
                            {"deadline", tra.deadline}};
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

auto from_json(const nlohmann::json& log) -> trace
{
        const std::map<std::string, traces::trace> convert{
            {"sim_finished", sim_finished{}},
            {"resched", resched{}},
            {"job_arrival", job_arrival{}},
            {"job_finished", job_finished{}},
            {"proc_activated", proc_activated{}},
            {"proc_idled", proc_idled{}},
            {"serv_budget_replenished", serv_budget_replenished{}},
            {"serv_inactive", serv_inactive{}},
            {"serv_running", serv_running{}},
            {"serv_budget_exhausted", serv_budget_exhausted{}},
            {"serv_non_cont", serv_non_cont{}},
            {"serv_postpone", serv_postpone{}},
            {"serv_ready", serv_ready{}},
            {"task_preempted", task_preempted{}},
            {"task_scheduled", task_scheduled{}},
            {"task_rejected", task_rejected{}},
            {"virtual_time_update", virtual_time_update{}},
            {"frequency_update", frequency_update{}}};

        auto search = convert.find(log.at("type").get<std::string>());
        if (search == std::end(convert)) { throw std::out_of_range("Unsupported event"); }

        trace out;

        std::visit(
            overloaded{
                [&out, &log](frequency_update) {
                        out = frequency_update{log.at("frequency").get<double>()};
                },
                [&out, &log](virtual_time_update) {
                        out = virtual_time_update{
                            log.at("tid").get<std::size_t>(), log.at("virtual_time").get<double>()};
                },
                [&out, &log](task_rejected) {
                        out = task_rejected{log.at("tid").get<std::size_t>()};
                },
                [&out, &log](task_scheduled) {
                        out = task_scheduled{
                            log.at("tid").get<std::size_t>(), log.at("cpu").get<std::size_t>()};
                },
                [&out, &log](task_preempted) {
                        out = task_preempted{log.at("tid").get<std::size_t>()};
                },
                [&out, &log](serv_running) {
                        out = serv_running{log.at("tid").get<std::size_t>()};
                },
                [&out, &log](serv_ready) {
                        out = serv_ready{
                            log.at("tid").get<std::size_t>(), log.at("deadline").get<double>()};
                },
                [&out, &log](serv_postpone) {
                        out = serv_postpone{
                            log.at("tid").get<std::size_t>(), log.at("deadline").get<double>()};
                },
                [&out, &log](serv_non_cont) {
                        out = serv_non_cont{log.at("tid").get<std::size_t>()};
                },
                [&out, &log](serv_budget_exhausted) {
                        out = serv_budget_exhausted{log.at("tid").get<std::size_t>()};
                },
                [&out, &log](serv_budget_replenished) {
                        out = serv_budget_replenished{
                            log.at("tid").get<std::size_t>(), log.at("budget").get<double>()};
                },
                [&out, &log](serv_inactive) {
                        out = serv_inactive{log.at("tid").get<std::size_t>()};
                },
                [&out, &log](proc_idled) {
                        out = proc_idled{.proc_id = log.at("cpu").get<std::size_t>()};
                },
                [&out, &log](proc_activated) {
                        out = proc_activated{log.at("cpu").get<std::size_t>()};
                },
                [&out, &log](job_finished) {
                        out = job_finished{log.at("tid").get<std::size_t>()};
                },
                [&out, &log](job_arrival) {
                        out = job_arrival{
                            log.at("tid").get<std::size_t>(),
                            log.at("duration").get<double>(),
                            log.at("deadline").get<double>()};
                },
                [&out](resched) { out = resched{}; },
                [&out](sim_finished) { out = sim_finished{}; }},
            search->second);
        return out;
}

void write_log_file(const std::multimap<double, trace>& logs, std::filesystem::path& file)
{
        std::ofstream out;
        out.open(file);

        /// Wait that the file is open
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

auto read_log_file(std::filesystem::path& file) -> std::multimap<double, trace>
{
        std::string input{};
        {
                std::ifstream input_file{file};

                /// Wait that the file is open
                while (!input_file.is_open()) {}
                std::ostringstream oss;
                oss << input_file.rdbuf();
                input = oss.str();
        }

        std::multimap<double, trace> parsed_traces{};

        auto json_input = nlohmann::json::parse(input);

        for (auto& json_trace : json_input) {
                parsed_traces.insert({json_trace.at("time").get<double>(), from_json(json_trace)});
        }

        return parsed_traces;
}
} // namespace protocols::traces
