#ifndef TRACE_HPP
#define TRACE_HPP

#include <cstddef>
#include <variant>

namespace traces {
struct resched {};
struct sim_finished {};

struct job_arrival {
        size_t task_id;
        double job_duration;
};

struct job_finished {
        size_t task_id;
};

struct proc_activated {
        size_t proc_id;
};

struct proc_idled {
        size_t proc_id;
};

struct serv_budget_replenished {
        size_t serv_id;
};

struct serv_inactive {
        size_t serv_id;
};

struct serv_budget_exhausted {
        size_t serv_id;
};

struct serv_non_cont {
        size_t serv_id;
};

struct serv_postpone {
        size_t serv_id;
        double new_deadline;
};

struct serv_ready {
        size_t serv_id;
        double deadline;
};

struct serv_running {
        size_t serv_id;
};

struct task_preempted {
        size_t task_id;
};

struct task_scheduled {
        size_t task_id;
        size_t proc_id;
};

struct task_rejected {
        size_t task_id;
};

struct virtual_time_update {
        size_t task_id;
        double new_virtual_time;
};

using trace = std::variant<sim_finished, resched, virtual_time_update, job_arrival, job_finished,
                           proc_activated, proc_idled, serv_budget_exhausted, serv_inactive,
                           serv_budget_replenished, serv_non_cont, serv_postpone, serv_ready,
                           serv_running, task_preempted, task_scheduled, task_rejected>;
}; // namespace traces
#endif
