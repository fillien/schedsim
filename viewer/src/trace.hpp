#ifndef TRACE_HPP
#define TRACE_HPP

#include <variant>

namespace traces {

struct serv_trace {
        std::size_t id;
};

struct resched {};
struct sim_finished {};
struct job_finished : public serv_trace {};
struct serv_inactive : public serv_trace {};
struct serv_budget_exhausted : public serv_trace {};
struct serv_non_cont : public serv_trace {};
struct serv_running : public serv_trace {};
struct task_rejected : public serv_trace {};
struct task_preempted : public serv_trace {};

struct job_arrival : public serv_trace {
        double job_duration;
};

struct serv_postpone : public serv_trace {
        double new_deadline;
};

struct serv_ready : public serv_trace {
        double deadline;
};

struct serv_budget_replenished : public serv_trace {
        double budget;
};

struct task_scheduled : public serv_trace {
        std::size_t proc_id;
};

struct virtual_time_update : public serv_trace {
        double new_virtual_time;
};

struct proc_activated {
        std::size_t id;
};

struct proc_idled {
        std::size_t id;
};

using trace = std::variant<
    sim_finished, resched, virtual_time_update, job_arrival, job_finished, proc_activated,
    proc_idled, serv_budget_exhausted, serv_inactive, serv_budget_replenished, serv_non_cont,
    serv_postpone, serv_ready, serv_running, task_preempted, task_scheduled, task_rejected>;
}; // namespace traces
#endif
