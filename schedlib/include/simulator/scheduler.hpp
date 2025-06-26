#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <simulator/entity.hpp>
#include <simulator/event.hpp>
#include <simulator/platform.hpp>
#include <simulator/processor.hpp>
#include <simulator/server.hpp>

#include <memory>
#include <vector>

namespace scheds {

/**
 * @brief Represents a scheduler responsible for managing tasks and servers within a simulation
 * environment.
 */
class Scheduler : public Entity, public std::enable_shared_from_this<Scheduler> {
      public:
        /**
         * @brief Constructs a scheduler with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit Scheduler(const std::weak_ptr<Engine> sim) : Entity(sim) {}
        virtual ~Scheduler() = default;
        Scheduler(const Scheduler&) = delete;
        auto operator=(const Scheduler&) -> Scheduler& = delete;
        Scheduler(Scheduler&&) noexcept = delete;
        auto operator=(Scheduler&&) noexcept -> Scheduler& = delete;

        /**
         * @brief Performs an admission test for a new task.
         * @param new_task The new task to test for admission.
         * @return True if the new task is admitted, false otherwise.
         */
        virtual auto admission_test(const Task& new_task) const -> bool = 0;

        /**
         * @brief Handles an event according to the scheduling policy.
         * @param evt The event to handle.
         */
        auto handle(const events::Event& evt) -> void;

        /**
         * @brief Triggers a rescheduling process.
         */
        auto call_resched() -> void;

        /**
         * @brief Determines if the event belongs to this scheduler.
         * @param evt The event to check.
         * @return True if the event is handled by this scheduler, false otherwise.
         */
        auto is_this_my_event(const events::Event& evt) -> bool;

        /**
         * @brief Sets the cluster.
         * @param clu Weak pointer to the cluster.
         */
        auto cluster(const std::weak_ptr<Cluster> clu) -> void { attached_cluster_ = clu; }

        /**
         * @brief Retrieves the cluster.
         * @return A shared pointer to the cluster.
         */
        [[nodiscard]] auto cluster() const -> std::shared_ptr<Cluster>
        {
                return attached_cluster_.lock();
        }

        /**
         * @brief Retrieves the maximum active utilization.
         * @return The maximum active utilization.
         */
        auto u_max() const -> double;

        /**
         * @brief Retrieves the active bandwidth of the system.
         * @return Active bandwidth of the system.
         */
        [[nodiscard]] auto active_bandwidth() const -> double;

        /**
         * @brief Handles the arrival of a job.
         * @param new_task The newly arrived task.
         * @param job_duration The job's duration.
         */
        auto on_job_arrival(const std::shared_ptr<Task>& new_task, const double& job_duration)
            -> void;

        auto servers() const -> const std::vector<std::shared_ptr<Server>>& { return servers_; }

        /**
         * @brief Retrieves the total system utilization.
         * @return Total utilization.
         */
        auto total_utilization() const -> double;

      protected:
        /**
         * @brief Retrieves the chip (cluster) associated with the scheduler.
         * @return A shared pointer to the cluster.
         */
        [[nodiscard]] auto chip() const -> std::shared_ptr<Cluster>;

        /**
         * @brief Checks if a server has a scheduled job.
         * @param serv The server to check.
         * @return True if the server has a job scheduled, false otherwise.
         */
        static auto has_job_server(const Server& serv) -> bool;

        /**
         * @brief Compares two servers based on their deadlines.
         * @param first The first server.
         * @param second The second server.
         * @return True if the first server has an earlier deadline, false otherwise.
         */
        static auto deadline_order(const Server& first, const Server& second) -> bool;

        /**
         * @brief Initiates rescheduling for a processor and server pair.
         * @param proc_with_server The processor holding a server.
         * @param server_to_execute The server to be executed.
         */
        auto resched_proc(
            const std::shared_ptr<Processor>& proc_with_server,
            const std::shared_ptr<Server>& server_to_execute) -> void;

        /**
         * @brief Updates timing information for a server.
         * @param serv The server to update.
         */
        auto update_server_times(const std::shared_ptr<Server>& serv) -> void;

        /**
         * @brief Updates the list of running servers.
         */
        auto update_running_servers() -> void;

        /**
         * @brief Cancels all future BUDGET_EXHAUSTED and JOB_FINISHED events for a server.
         * @param serv The server whose alarms are canceled.
         */
        auto cancel_alarms(const Server& serv) -> void;

        /**
         * @brief Sets alarms for a server to track BUDGET_EXHAUSTED and JOB_FINISHED events.
         * @param serv The server for which alarms are set.
         */
        auto activate_alarms(const std::shared_ptr<Server>& serv) -> void;

        /**
         * @brief Clamps the requested number of processors between 1 and the maximum available.
         * @param nb_procs The requested number of processors.
         * @return The clamped number.
         */
        auto clamp(const double& nb_procs) -> double;

        /**
         * @brief Retrieves the virtual time of a server based on its running time.
         * @param serv The server.
         * @param running_time The server's running time.
         * @return The calculated virtual time.
         */
        virtual auto server_virtual_time(const Server& serv, const double& running_time)
            -> double = 0;

        /**
         * @brief Retrieves the budget of a server.
         * @param serv The server.
         * @return The server's budget.
         */
        virtual auto server_budget(const Server& serv) const -> double = 0;

        /**
         * @brief Custom scheduling logic to be implemented by derived classes.
         */
        virtual auto on_resched() -> void = 0;

        virtual auto on_active_utilization_updated() -> void = 0;

        virtual auto update_platform() -> void = 0;

      private:
        /**
         * @brief Handles the completion of a job on a server.
         * @param serv The server where the job finished.
         * @param is_there_new_job Flag indicating if a new job should be scheduled.
         */
        auto on_job_finished(const std::shared_ptr<Server>& serv, bool is_there_new_job) -> void;

        /**
         * @brief Handles a server's budget exhaustion.
         * @param serv The server with an exhausted budget.
         */
        auto on_serv_budget_exhausted(const std::shared_ptr<Server>& serv) -> void;

        /**
         * @brief Handles a server's inactivity.
         * @param serv The inactive server.
         */
        auto on_serv_inactive(const std::shared_ptr<Server>& serv) -> void;

        /**
         * @brief Detaches a server if its associated task becomes inactive.
         * @param inactive_task The inactive task.
         */
        auto detach_server_if_needed(const std::shared_ptr<Task>& inactive_task) -> void;

        std::vector<std::shared_ptr<Server>> servers_;
        std::weak_ptr<Cluster> attached_cluster_;

        double total_utilization_{0.0};
};

} // namespace scheds

#endif // SCHEDULER_HPP
