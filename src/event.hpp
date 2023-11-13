#ifndef EVENT_HPP
#define EVENT_HPP

#include "entity.hpp"
#include "processor.hpp"
#include "server.hpp"
#include "task.hpp"

#include <memory>
#include <ostream>
#include <variant>
#include <vector>

namespace events {

struct resched {};
struct sim_finished {};

struct job_arrival {
        std::shared_ptr<task> task_of_job;
        double job_duration;
};

struct job_finished {
        std::shared_ptr<server> server_of_job;
};

struct proc_activated {
        std::shared_ptr<processor> proc;
};

struct proc_idled {
        std::shared_ptr<processor> proc;
};

struct serv_budget_replenished {
        std::shared_ptr<server> serv;
        double budget;
};

struct serv_inactive {
        std::shared_ptr<server> serv;
};

struct serv_budget_exhausted {
        std::shared_ptr<server> serv;
};

struct serv_non_cont {
        std::shared_ptr<server> serv;
};

struct serv_postpone {
        std::shared_ptr<server> serv;
        double new_deadline;
};

struct serv_ready {
        std::shared_ptr<server> serv;
        double deadline;
};

struct serv_running {
        std::shared_ptr<server> serv;
};

struct task_preempted {
        std::shared_ptr<task> the_task;
};

struct task_scheduled {
        std::shared_ptr<task> sched_task;
        std::shared_ptr<processor> proc;
};

struct task_rejected {
        std::shared_ptr<task> the_task;
};

struct virtual_time_update {
        std::shared_ptr<task> the_task;
        double new_virtual_time;
};

using event = std::variant<
    resched, sim_finished, virtual_time_update, job_arrival, job_finished, proc_activated,
    proc_idled, serv_budget_exhausted, serv_inactive, serv_budget_replenished, serv_non_cont,
    serv_postpone, serv_ready, serv_running, task_preempted, task_scheduled, task_rejected>;
}; // namespace events

#endif
