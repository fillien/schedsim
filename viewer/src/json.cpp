#include "json.hpp"
#include "nlohmann/json_fwd.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void inputs::json::parse(
    const std::string& input_text, std::vector<std::pair<double, traces::trace>>& out)
{
        auto json_input = nlohmann::json::parse(input_text);

        for (const auto& json_trace : json_input) {
                out.push_back({json_trace.at("time").get<double>(), parse_trace(json_trace)});
        }
}

auto inputs::json::parse_trace(nlohmann::json trace) -> traces::trace
{
        using namespace traces;

        static const std::map<std::string, traces::trace> to_trace{
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
            {"virtual_time_update", virtual_time_update{}}};

        auto search = to_trace.find(trace.at("type").get<std::string>());
        if (search == std::end(to_trace)) { throw std::out_of_range("Unsupported event"); }

        traces::trace out;

        std::visit(
            overloaded{
                [&out, &trace](virtual_time_update) {
                        out = virtual_time_update{
                            trace.at("tid").get<std::size_t>(),
                            trace.at("virtual_time").get<double>()};
                },
                [&out, &trace](task_rejected) {
                        out = task_rejected{trace.at("tid").get<std::size_t>()};
                },
                [&out, &trace](task_scheduled) {
                        out = task_scheduled{
                            trace.at("tid").get<std::size_t>(), trace.at("cpu").get<std::size_t>()};
                },
                [&out, &trace](task_preempted) {
                        out = task_preempted{trace.at("tid").get<std::size_t>()};
                },
                [&out, &trace](serv_running) {
                        out = serv_running{trace.at("tid").get<std::size_t>()};
                },
                [&out, &trace](serv_ready) {
                        out = serv_ready{
                            trace.at("tid").get<std::size_t>(), trace.at("deadline").get<double>()};
                },
                [&out, &trace](serv_postpone) {
                        out = serv_postpone{
                            trace.at("tid").get<std::size_t>(), trace.at("deadline").get<double>()};
                },
                [&out, &trace](serv_non_cont) {
                        out = serv_non_cont{trace.at("tid").get<std::size_t>()};
                },
                [&out, &trace](serv_budget_exhausted) {
                        out = serv_budget_exhausted{trace.at("tid").get<std::size_t>()};
                },
                [&out, &trace](serv_budget_replenished) {
                        out = serv_budget_replenished{
                            trace.at("tid").get<std::size_t>(), trace.at("budget").get<double>()};
                },
                [&out, &trace](serv_inactive) {
                        out = serv_inactive{trace.at("tid").get<std::size_t>()};
                },
                [&out, &trace](proc_idled) {
                        out = proc_idled{.id = trace.at("cpu").get<std::size_t>()};
                },
                [&out, &trace](proc_activated) {
                        out = proc_activated{trace.at("cpu").get<std::size_t>()};
                },
                [&out, &trace](job_finished) {
                        out = job_finished{trace.at("tid").get<std::size_t>()};
                },
                [&out, &trace](job_arrival) {
                        out = job_arrival{
                            trace.at("tid").get<std::size_t>(), trace.at("duration").get<double>()};
                },
                [&out](resched) { out = resched{}; },
                [&out](sim_finished) { out = sim_finished{}; }, [](auto&) {}},
            search->second);
        return out;
}
