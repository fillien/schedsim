#ifndef SCHED_PARALLEL_HPP
#define SCHED_PARALLEL_HPP

#include <engine.hpp>
#include <entity.hpp>
#include <memory>
#include <processor.hpp>
#include <scheduler.hpp>

namespace scheds {

/**
 * @brief A parallel scheduler that implements custom scheduling logic
 *        for systems with parallel processing capabilities.
 */
class Parallel : public Scheduler {
      public:
        /**
         * @brief Constructs a parallel scheduler with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit Parallel(const std::weak_ptr<Engine>& sim) : Scheduler(sim) {}

        /**
         * @brief Compares two processors based on their order.
         * @param first The first processor.
         * @param second The second processor.
         * @return True if the first processor should be scheduled before the second, false
         * otherwise.
         */
        static auto processor_order(const Processor& first, const Processor& second) -> bool;

        /**
         * @brief Removes the task from the specified processor.
         * @param proc The processor from which the task will be removed.
         */
        auto remove_task_from_cpu(const std::shared_ptr<Processor>& proc) -> void;

        /**
         * @brief Retrieves the budget of a server for the parallel scheduler.
         * @param serv The server for which to retrieve the budget.
         * @return The budget of the server.
         */
        auto server_budget(const Server& serv) const -> double override;

        /**
         * @brief Calculates the virtual time of a server for the parallel scheduler.
         * @param serv The server.
         * @param running_time The running time of the server.
         * @return The calculated virtual time.
         */
        auto server_virtual_time(const Server& serv, const double& running_time) -> double override;

        /**
         * @brief Performs an admission test for a new task in the parallel scheduler.
         * @param new_task The task to test.
         * @return True if the task is admitted, false otherwise.
         */
        auto admission_test(const Task& new_task) const -> bool override;

        /**
         * @brief Executes the custom scheduling logic.
         */
        auto on_resched() -> void override;

        /**
         * @brief Callback when active utilization is updated (no action required for parallel
         * scheduling).
         */
        auto on_active_utilization_updated() -> void override {}

        /**
         * @brief Updates the simulation platform.
         */
        auto update_platform() -> void override;

      protected:
        /**
         * @brief Retrieves the inactive bandwidth of the system.
         * @return The inactive bandwidth.
         */
        auto inactive_bandwidth() const -> double;

        /**
         * @brief Retrieves the number of active processors in the system.
         * @param new_utilization The additional utilization to consider (default is 0).
         * @return The number of active processors.
         */
        auto nb_active_procs() const -> std::size_t;
};

} // namespace scheds

#endif // SCHED_PARALLEL_HPP
