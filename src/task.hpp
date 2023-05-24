#ifndef TASK_HPP
#define TASK_HPP

#include "entity.hpp"
#include <memory>

class processor;

/**
 * @brief task A model of a user code that is executed by a processor
 */
class task : public entity {
      public:
        /**
         * @brief A unique id
         */
        int id;
        /**
         * @brief
         */
        double period;

        /**
         * @brief Utilisation of the processor when the task is active.
         */
        double utilization;

        /**
         * @brief A remaining duration of time that the processor have to execute.
         * @description When job arrive in the system, the value of this variable is increased by
         * the duration of the job that arrived.
         */
        double remaining_execution_time{0};

        /**
         * @brief A constructor with a unique id, the period and the utilization.
         * @param id A unique id.
         * @param period The period of the task.
         * @param utilization The utilization tacken when active.
         */
        task(const int id, const double& period, const double& utilization);

        /**
         * @brief Return true if the task is currently attached to a processor
         */
        auto is_attached() -> bool;

        /**
         * @brief Return true if the task has remaining time to be executed
         */
        auto has_remaining_time() -> bool;

      private:
        /**
         * @brief The processor on which the task is executed.
         */
        std::shared_ptr<processor> attached_proc{nullptr};
};

#endif
