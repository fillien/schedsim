#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "entity.hpp"
#include "event.hpp"
#include "server.hpp"
#include <memory>
#include <vector>

/**
 * @brief A class that handle the events of the system accordingly to a scheduling policy.
 */
class scheduler : public entity {
      private:
        bool need_resched{false};

      protected:
        /**
         * @brief A vector to track and own servers objects
         */
        std::vector<std::shared_ptr<server>> servers;

        static auto is_running_server(const std::shared_ptr<server>& current_server) -> bool;
        static auto is_ready_server(const std::shared_ptr<server>& current_server) -> bool;
        static auto is_active_server(const std::shared_ptr<server>& current_server) -> bool;
        static auto deadline_order(const std::shared_ptr<server>& first,
                                   const std::shared_ptr<server>& second) -> bool;

        /**
         * @brief Helper function to add a new trace to the logs.
         * @param type The kind of event
         * @param target_id Id of the object that is link to the kind of event
         * @param payload A optionnal payload
         */
        void add_trace(types type, int target_id, double payload = 0) const;

        /**
         * @brief Check if a type of event is present at the current timestamp and act on a task
         * @param the_task The task on which the event is applied
         * @param type The type of event to looking for
         */
        auto is_event_present(const std::shared_ptr<task>& the_task, types type) -> bool;

        void handle_job_arrival(const std::shared_ptr<task>& new_task, const double& job_wcet);
        void handle_job_finished(const std::shared_ptr<server>& serv, bool is_there_new_job);
        void handle_serv_budget_exhausted(const std::shared_ptr<server>& serv);
        void handle_serv_inactive(const std::shared_ptr<server>& serv);
        void resched();

        void resched_proc(const std::shared_ptr<processor>& proc_with_server,
                          const std::shared_ptr<server>& server_to_execute);

        void update_server_times(const std::shared_ptr<server>& serv);

        virtual auto get_server_new_virtual_time(const std::shared_ptr<server>& serv,
                                                 const double& running_time) -> double = 0;
        virtual auto get_server_budget(const std::shared_ptr<server>& serv) -> double = 0;
        virtual auto admission_test(const std::shared_ptr<task>& new_task) -> bool = 0;
        virtual void custom_scheduler() = 0;

      public:
        explicit scheduler(const std::weak_ptr<engine> sim) : entity(sim){};
        virtual ~scheduler() = default;

        void handle(std::vector<event> evts);
        auto get_active_bandwidth() -> double;
        auto make_server(const std::shared_ptr<task>& new_task) -> std::shared_ptr<server>;
};

#endif
