#ifndef TRACES_HPP
#define TRACES_HPP

#include "nlohmann/json.hpp"
#include <cstdint>
#include <filesystem>
#include <map>
#include <variant>
#include <vector>

namespace traces {
struct resched {};
struct sim_finished {};

struct job_arrival {
        uint16_t task_id;
        double duration;
};

struct job_finished {
        uint16_t task_id;
};

struct proc_activated {
        uint16_t proc_id;
};

struct proc_idled {
        uint16_t proc_id;
};

struct serv_budget_replenished {
        uint16_t task_id;
        double budget;
};

struct serv_inactive {
        uint16_t task_id;
};

struct serv_budget_exhausted {
        uint16_t task_id;
};

struct serv_non_cont {
        uint16_t task_id;
};

struct serv_postpone {
        uint16_t task_id;
        double deadline;
};

struct serv_ready {
        uint16_t task_id;
        double deadline;
};

struct serv_running {
        uint16_t task_id;
};

struct task_preempted {
        uint16_t task_id;
};

struct task_scheduled {
        uint16_t task_id;
        uint16_t proc_id;
};

struct task_rejected {
        uint16_t task_id;
};

struct virtual_time_update {
        uint16_t task_id;
        double virtual_time;
};

using trace = std::variant<
    resched, sim_finished, virtual_time_update, job_arrival, job_finished, proc_activated,
    proc_idled, serv_budget_exhausted, serv_inactive, serv_budget_replenished, serv_non_cont,
    serv_postpone, serv_ready, serv_running, task_preempted, task_scheduled, task_rejected>;

auto to_json(const trace& log) -> nlohmann::json;
auto from_json(const nlohmann::json& log) -> trace;

void write_log_file(const std::multimap<double, trace>& logs, std::filesystem::path& file);
auto read_log_file(std::filesystem::path& file) -> std::multimap<double, trace>;

} // namespace traces

#endif
