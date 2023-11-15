#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "entity.hpp"
#include "event.hpp"
#include "processor.hpp"
#include "server.hpp"
#include <memory>
#include <vector>

/**
 * @brief A class that handle the events of the system accordingly to a scheduling policy.
 */
class scheduler : public entity {
      private:
        bool need_resched{false};
        double total_utilization{0};

        void handle_job_arrival(const std::shared_ptr<task>& new_task, const double& job_duration);
        void handle_job_finished(const std::shared_ptr<server>& serv, bool is_there_new_job);
        void handle_serv_budget_exhausted(const std::shared_ptr<server>& serv);
        void handle_serv_inactive(const std::shared_ptr<server>& serv);
        void resched();
        void detach_server_if_needed(const std::shared_ptr<task>& inactive_task);

      protected:
        /**
         * @brief A vector to track and own servers objects
         */
        std::vector<std::shared_ptr<server>> servers;

        static auto is_running_server(const server& serv) -> bool;
        static auto is_ready_server(const server& serv) -> bool;
        static auto is_active_server(const server& serv) -> bool;
        static auto has_job_server(const server& serv) -> bool;
        static auto deadline_order(const server& first, const server& second) -> bool;
        auto get_total_utilization() const -> double { return total_utilization; };

        void resched_proc(
            const std::shared_ptr<processor>& proc_with_server,
            const std::shared_ptr<server>& server_to_execute);
        void update_server_times(const std::shared_ptr<server>& serv);
        void update_running_servers();

        /**
         * @brief Remove all future event of type BUDGET_EXHAUSTED and JOB_FINISHED of the given
         * server
         * @param serv The server with alarms
         */
        void cancel_alarms(const server& serv);
        void set_alarms(const std::shared_ptr<server>& serv);

        virtual auto get_server_virtual_time(const server& serv, const double& running_time)
            -> double = 0;
        virtual auto get_server_budget(const server& serv) const -> double = 0;
        virtual auto admission_test(const task& new_task) const -> bool = 0;
        virtual void custom_scheduler() = 0;

      public:
        explicit scheduler(const std::weak_ptr<engine> sim) : entity(sim){};
        virtual ~scheduler() = default;

        void handle(std::vector<events::event> evts);
        auto get_active_bandwidth() const -> double;
};

#endif
