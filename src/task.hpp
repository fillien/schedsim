#ifndef TASK_HPP
#define TASK_HPP

#include "entity.hpp"
#include <memory>
#include <queue>

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
         * @brief The processor on which the task is executed.
         */
        std::shared_ptr<processor> attached_proc{};

        /**
         * @brief A constructor with a unique id, the period and the utilization.
         * @param id A unique id.
         * @param period The period of the task.
         * @param utilization The utilization tacken when active.
         */
        task(const std::weak_ptr<engine>& sim, int tid, const double& period,
             const double& utilization);

        /**
         * @brief Return true if the task is currently attached to a processor
         */
        auto is_attached() const -> bool;

        /**
         * @brief Return true if the task has remaining time to be executed
         */
        auto has_remaining_time() const -> bool;

        /**
         * @brief Add new job to the queue.
         * @param duration The duration of the new job
         */
        void add_job(const double& duration);

        void consume_time(const double& duration);

        /**
         * @brief Return remaining execution time
         */
        auto get_remaining_time() const -> double { return remaining_execution_time; };

        auto has_job() const -> bool { return !pending_jobs.empty(); };

        void next_job();

      private:
        /**
         * @brief A remaining duration of time that the processor have to execute.
         * @description When job arrive in the system, the value of this variable is increased by
         * the duration of the job that arrived.
         */
        double remaining_execution_time{0};

        /**
         * @brief Queue of wcet of the pending jobs
         * @description When a job arrive, his wcet is store in the queue. When the task finish a
         * job, the remaining_execution_time is set to the next job wcet.
         */
        std::queue<double> pending_jobs;
};

#endif
