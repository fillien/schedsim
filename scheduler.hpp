#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "event.hpp"
#include "server.hpp"
#include "tracer.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <vector>

class engine;

class scheduler {
        std::weak_ptr<engine> simulator;
        std::vector<std::shared_ptr<server>> servers;

        void add_trace(const types type, const int target_id, const double payload = 0) const;
        auto is_event_present(const std::shared_ptr<task>& the_task, const types type) -> bool;
        auto get_active_bandwidth() -> double;
        auto compute_budget(const server&) -> double;
        void update_server_time(const std::shared_ptr<server>& serv, const double time_comsumed);
        void postpone(const std::shared_ptr<server>& serv);
        void goto_active_cont(const std::shared_ptr<server>& serv);
        void goto_active_non_cont(const std::shared_ptr<server>& serv);
        void goto_idle(const std::shared_ptr<server>& serv);

        auto sim() const -> std::shared_ptr<engine> {
                assert(!simulator.expired());
                return simulator.lock();
        }

      public:
        void set_engine(std::weak_ptr<engine> simulator);

        /// Mandatory event handler
        void handle_undefined_event(const event& evt);

        /// Used event handlers
        void handle_proc_activated(const event& evt);
        void handle_proc_idle(const event& evt);
        void handle_serv_active_cont(const event& evt);
        void handle_serv_active_non_cont(const event& evt);
        void handle_serv_budget_exhausted(const event& evt);
        void handle_serv_budget_replenished(const event& evt);
        void handle_serv_idle(const event& evt);
        void handle_serv_running(const event& evt);
        void handle_task_preempted(const event& evt);
        void handle_task_scheduled(const event& evt);
        void handle_job_arrival(const event& evt);
        void handle_job_finished(const event& evt);
        void handle_resched(const event& evt);
        void handle_sim_finished(const event& evt);
};

#endif
