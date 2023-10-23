#include "parse_trace.hpp"
#include "trace.hpp"
#include <algorithm>
#include <cstddef>
#include <string>
#include <variant>

#include <iostream>

auto parse_trace(nlohmann::json trace) -> traces::trace {
        using namespace traces;

        static const std::map<std::string, traces::trace> toTrace{
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

        /// TODO handle errors
        auto search = toTrace.find(trace.at("type").get<std::string>());

        traces::trace out;

        if (std::holds_alternative<sim_finished>(search->second)) {
                out = sim_finished{};
        } else if (std::holds_alternative<resched>(search->second)) {
                out = resched{};
        } else if (std::holds_alternative<job_arrival>(search->second)) {
                out = job_arrival{.task_id = trace.at("tid").get<size_t>(),
                                  .job_duration = trace.at("duration").get<double>()};
        } else if (std::holds_alternative<job_finished>(search->second)) {
                out = job_finished{.task_id = trace.at("tid").get<size_t>()};
        } else if (std::holds_alternative<proc_activated>(search->second)) {
                out = proc_activated{.proc_id = trace.at("cpu").get<size_t>()};
        } else if (std::holds_alternative<proc_idled>(search->second)) {
                out = proc_idled{.proc_id = trace.at("cpu").get<size_t>()};
        } else if (std::holds_alternative<serv_inactive>(search->second)) {
                out = serv_inactive{.serv_id = trace.at("tid").get<size_t>()};
        } else if (std::holds_alternative<serv_budget_replenished>(search->second)) {
                out = serv_budget_replenished{
                    .serv_id = trace.at("tid").get<size_t>(),
                };
        } else if (std::holds_alternative<serv_budget_exhausted>(search->second)) {
                out = serv_budget_exhausted{.serv_id = trace.at("tid").get<size_t>()};
        } else if (std::holds_alternative<serv_non_cont>(search->second)) {
                out = serv_non_cont{.serv_id = trace.at("tid").get<size_t>()};
        } else if (std::holds_alternative<serv_postpone>(search->second)) {
                out = serv_postpone{.serv_id = trace.at("tid").get<size_t>(),
                                    .new_deadline = trace.at("deadline").get<double>()};
        } else if (std::holds_alternative<serv_ready>(search->second)) {
                out = serv_ready{.serv_id = trace.at("tid").get<size_t>(),
                                 .deadline = trace.at("deadline").get<double>()};
        } else if (std::holds_alternative<serv_running>(search->second)) {
                out = serv_running{.serv_id = trace.at("tid").get<size_t>()};
        } else if (std::holds_alternative<task_preempted>(search->second)) {
                out = task_preempted{.task_id = trace.at("tid").get<size_t>()};
        } else if (std::holds_alternative<task_scheduled>(search->second)) {
                out = task_scheduled{.task_id = trace.at("tid").get<size_t>(),
                                     .proc_id = trace.at("cpu").get<size_t>()};
        } else if (std::holds_alternative<task_rejected>(search->second)) {
                out = task_rejected{.task_id = trace.at("tid").get<size_t>()};
        } else if (std::holds_alternative<virtual_time_update>(search->second)) {
                out =
                    virtual_time_update{.task_id = trace.at("tid").get<size_t>(),
                                        .new_virtual_time = trace.at("virtual_time").get<double>()};
        }

        return out;
}
