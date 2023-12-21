#ifndef SCHED_PARALLEL_HPP
#define SCHED_PARALLEL_HPP

#include "entity.hpp"
#include "processor.hpp"
#include "scheduler.hpp"
#include <iostream>
#include <memory>

/**
 * @brief A class implementing a parallel scheduler, derived from the base scheduler class.
 */
class sched_parallel : public scheduler {
        /**
         * @brief Retrieves the inactive bandwidth of the system.
         * @return Inactive bandwidth of the system.
         */
        auto get_inactive_bandwidth() const -> double;

        /**
         * @brief Retrieves the number of active processors in the system.
         * @param new_utilization The additional utilization to consider (default is 0).
         * @return Number of active processors.
         */
        auto get_nb_active_procs(const double& new_utilization = 0) const -> std::size_t;

        /**
         * @brief Compares two processors based on their order.
         * @param first The first processor.
         * @param second The second processor.
         * @return True if the first processor should be scheduled before the second, false
         * otherwise.
         */
        static auto processor_order(const processor& first, const processor& second) -> bool;

      public:
        /**
         * @brief Constructs a parallel scheduler with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit sched_parallel(const std::weak_ptr<engine> sim) : scheduler(sim){};

        /**
         * @brief Retrieves the budget of a server for the parallel scheduler.
         * @param serv The server for which to retrieve the budget.
         * @return Budget of the server.
         */
        auto get_server_budget(const server& serv) const -> double override;

        /**
         * @brief Retrieves the virtual time of a server for the parallel scheduler.
         * @param serv The server for which to calculate virtual time.
         * @param running_time The running time of the server.
         * @return Calculated virtual time.
         */
        auto get_server_virtual_time(const server& serv, const double& running_time)
            -> double override;

        /**
         * @brief Performs an admission test for a new task in the parallel scheduler.
         * @param new_task The new task to test for admission.
         * @return True if the new task is admitted, false otherwise.
         */
        auto admission_test(const task& new_task) const -> bool override;

        /**
         * @brief Implements the custom scheduling logic for the parallel scheduler.
         */
        void custom_scheduler() override;
};

#endif