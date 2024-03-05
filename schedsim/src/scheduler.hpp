#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "entity.hpp"
#include "event.hpp"
#include "processor.hpp"
#include "server.hpp"
#include <memory>
#include <vector>

/**
 * @brief Compares two events based on their timestamps.
 * @param ev1 The first event.
 * @param ev2 The second event.
 * @return True if the first event occurs before the second, false otherwise.
 */
auto compare_events(const events::event& ev1, const events::event& ev2) -> bool;

/**
 * @brief A class that handles the events of the system according to a scheduling policy.
 */
class scheduler : public entity {
      private:
        bool need_resched{false};    /**< Flag indicating if a rescheduling is needed. */
        double total_utilization{0}; /**< Total utilization of the system. */

        /**
         * @brief Handles the arrival of a job.
         * @param new_task The newly arrived task.
         * @param job_duration The duration of the job.
         */
        void on_job_arrival(const std::shared_ptr<task>& new_task, const double& job_duration);

        /**
         * @brief Handles the completion of a job on a server.
         * @param serv The server where the job is completed.
         * @param is_there_new_job Flag indicating if there is a new job to be scheduled.
         */
        void on_job_finished(const std::shared_ptr<server>& serv, bool is_there_new_job);

        /**
         * @brief Handles the exhaustion of the budget on a server.
         * @param serv The server with an exhausted budget.
         */
        void on_serv_budget_exhausted(const std::shared_ptr<server>& serv);

        /**
         * @brief Handles the inactivity of a server.
         * @param serv The inactive server.
         */
        void on_serv_inactive(const std::shared_ptr<server>& serv);

        /**
         * @brief Detaches a server if needed, i.e., if its attached task is inactive.
         * @param inactive_task The inactive task associated with the server.
         */
        void detach_server_if_needed(const std::shared_ptr<task>& inactive_task);

      protected:
        /**
         * @brief A vector to track and own server objects.
         */
        std::vector<std::shared_ptr<server>> servers;

        /**
         * @brief Checks if a server is currently running.
         * @param serv The server to check.
         * @return True if the server is running, false otherwise.
         */
        static auto is_running_server(const server& serv) -> bool;

        /**
         * @brief Checks if a server is ready for execution.
         * @param serv The server to check.
         * @return True if the server is ready, false otherwise.
         */
        static auto is_ready_server(const server& serv) -> bool;

        /**
         * @brief Checks if a server has a job to execute.
         * @param serv The server to check.
         * @return True if the server has a job, false otherwise.
         */
        static auto is_active_server(const server& serv) -> bool;

        /**
         * @brief Checks if a server has a job to execute.
         * @param serv The server to check.
         * @return True if the server has a job, false otherwise.
         */
        static auto has_job_server(const server& serv) -> bool;
        /**
         * @brief Compares two servers based on their deadlines.
         * @param first The first server.
         * @param second The second server.
         * @return True if the first server has an earlier deadline, false otherwise.
         */
        static auto deadline_order(const server& first, const server& second) -> bool;

        /**
         * @brief Retrieves the total utilization of the system.
         * @return Total utilization of the system.
         */
        auto get_total_utilization() const -> double;

        /**
         * @brief Initiates a rescheduling process for a processor and server pair.
         * @param proc_with_server The processor with a server.
         * @param server_to_execute The server to be executed on the processor.
         */
        void resched_proc(
            const std::shared_ptr<processor>& proc_with_server,
            const std::shared_ptr<server>& server_to_execute);

        /**
         * @brief Updates the timing information for a server.
         * @param serv The server to update.
         */
        void update_server_times(const std::shared_ptr<server>& serv);

        /**
         * @brief Updates the list of running servers in the system.
         */
        void update_running_servers();

        /**
         * @brief Cancels all future events of type BUDGET_EXHAUSTED and JOB_FINISHED for a given
         * server.
         * @param serv The server with alarms to cancel.
         */
        void cancel_alarms(const server& serv);

        /**
         * @brief Sets alarms for a server to track BUDGET_EXHAUSTED and JOB_FINISHED events.
         * @param serv The server for which to set alarms.
         */
        void set_alarms(const std::shared_ptr<server>& serv);

        /**
         * @brief Retrieves the virtual time of a server given its running time.
         * @param serv The server for which to calculate virtual time.
         * @param running_time The running time of the server.
         * @return Calculated virtual time.
         */
        virtual auto get_server_virtual_time(const server& serv, const double& running_time)
            -> double = 0;

        /**
         * @brief Retrieves the budget of a server.
         * @param serv The server for which to retrieve the budget.
         * @return Budget of the server.
         */
        virtual auto get_server_budget(const server& serv) const -> double = 0;

        /**
         * @brief Performs an admission test for a new task.
         * @param new_task The new task to test for admission.
         * @return True if the new task is admitted, false otherwise.
         */
        virtual auto admission_test(const task& new_task) const -> bool = 0;

        /**
         * @brief Custom scheduling logic to be implemented by derived classes.
         */
        virtual void on_resched() = 0;

        virtual void on_active_utilization_updated() = 0;

      public:
        /**
         * @brief Constructs a scheduler with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit scheduler(const std::weak_ptr<engine> sim) : entity(sim){};

        /**
         * @brief Virtual destructor for the scheduler class.
         */
        virtual ~scheduler() = default;

        /**
         * @brief Handles a vector of events according to the scheduling policy.
         * @param evts Vector of events to handle.
         */
        void handle(std::vector<events::event> evts);

        /**
         * @brief Retrieves the active bandwidth of the system.
         * @return Active bandwidth of the system.
         */
        auto get_active_bandwidth() const -> double;
};

#endif
