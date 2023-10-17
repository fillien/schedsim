#ifndef TRACER_JSON
#define TRACER_JSON

#include "event.hpp"
#include "nlohmann/json.hpp"

#include <iostream>
#include <variant>

struct log_json {
        using json = nlohmann::json;

        auto operator()(const events::job_arrival& evt) const {
                return json{{"type", "job_arrival"},
                            {"tid", evt.task_of_job->id},
                            {"duration", evt.job_duration}};
        };
        auto operator()(const events::job_finished& evt) const {
                return json{{"type", "job_finished"}, {"tid", evt.server_of_job->id()}};
        };
        auto operator()(const events::proc_idled& evt) const {
                return json{{"type", "proc_idled"}, {"cpu", evt.proc->get_id()}};
        };
        auto operator()(const events::proc_activated& evt) const {
                return json{{"type", "proc_activated"}, {"cpu", evt.proc->get_id()}};
        };
        auto operator()([[maybe_unused]] const events::resched& evt) const {
                return json{{"type", "resched"}};
        }
        auto operator()(const events::serv_non_cont& evt) {
                return json{{"type", "serv_non_cont"}, {"tid", evt.serv->id()}};
        };
        auto operator()(const events::serv_budget_exhausted& evt) const {
                return json{{"type", "serv_budget_exhausted"}, {"tid", evt.serv->id()}};
        };
        auto operator()(const events::serv_budget_replenished& evt) const {
                return json{{"type", "serv_budget_replenished"}, {"tid", evt.serv->id()}};
        };
        auto operator()(const events::serv_inactive& evt) const {
                return json{{"type", "serv_inactive"}, {"tid", evt.serv->id()}};
        };
        auto operator()(const events::serv_postpone& evt) const {
                return json{{"type", "serv_postpone"},
                            {"tid", evt.serv->id()},
                            {"deadline", evt.new_deadline}};
        };
        auto operator()(const events::serv_ready& evt) const {
                return json{{"type", "serv_ready"}, {"tid", evt.serv->id()}};
        };
        auto operator()(const events::serv_running& evt) const {
                return json{{"type", "serv_running"}, {"tid", evt.serv->id()}};
        };
        auto operator()(const events::task_preempted& evt) const {
                return json{{"type", "task_preempted"}, {"tid", evt.the_task->id}};
        };
        auto operator()(const events::task_scheduled& evt) const {
                return json{{"type", "task_scheduled"},
                            {"tid", evt.sched_task->id},
                            {"cpu", evt.proc->get_id()}};
        };
        auto operator()(const events::task_rejected& evt) const {
                return json{{"type", "task_rejected"}, {"tid", evt.the_task->id}};
        };
        auto operator()(const events::virtual_time_update& evt) const {
                return json{{"type", "virtual_time_update"},
                            {"tid", evt.the_task->id},
                            {"virtual_time", evt.new_virtual_time}};
        };
        auto operator()([[maybe_unused]] const events::sim_finished& evt) const {
                return json{{"type", "sim_finished"}};
        };
};

auto print_json(const std::pair<double, events::event>& evt) -> std::string;

#endif
