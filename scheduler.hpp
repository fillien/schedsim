#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "event.hpp"
#include "tracer.hpp"

#include <functional>
#include <memory>
#include <vector>

class engine;

class scheduler {
        std::weak_ptr<engine> simulator;
        std::vector<std::shared_ptr<server>> servers;

        void add_trace(const trace::types& new_trace) const;
        auto get_active_bandwidth() -> double;
        auto compute_budget(const server&) -> double;

      public:
        void set_engine(std::weak_ptr<engine> simulator);

        /// Mandatory event handler
        void handle_undefined_event(const event& evt);

        /// Used event handlers
        void handle_proc_activated(const event& evt);
        void handle_proc_idle(const event& evt);
        void handle_proc_stopped(const event& evt);
        void handle_proc_stopping(const event& evt);
        void handle_serv_active_cont(const event& evt);
        void handle_serv_active_non_cont(const event& evt);
        void handle_serv_budget_exhausted(const event& evt);
        void handle_serv_budget_replenished(const event& evt);
        void handle_serv_idle(const event& evt);
        void handle_serv_running(const event& evt);
        void handle_task_blocked(const event& evt);
        void handle_task_killed(const event& evt);
        void handle_task_preempted(const event& evt);
        void handle_task_scheduled(const event& evt);
        void handle_task_unblocked(const event& evt);
        void handle_job_arrival(const event& evt);
        void handle_job_finished(const event& evt);
        void handle_resched(const event& evt);

        /// Unused event handlers
        void handle_plat_speed_changed(const event& evt [[maybe_unused]]){};
        void handle_sim_finished(const event& evt [[maybe_unused]]){};
        void handle_task_migrated(const event& evt [[maybe_unused]]){};
        void handle_tt_mode(const event& evt [[maybe_unused]]){};
};

#endif
