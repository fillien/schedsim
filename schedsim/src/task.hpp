#ifndef TASK_HPP
#define TASK_HPP

#include "entity.hpp"
#include <cstddef>
#include <memory>
#include <queue>

class processor;
class server;

/**
 * @brief Represents a model of a user code that is executed by a processor.
 */
class task : public entity, public std::enable_shared_from_this<task> {
      public:
        /**
         * @brief A unique ID for the task.
         */
        std::size_t id;

        /**
         * @brief The period of the task.
         */
        double period;

        /**
         * @brief Utilization of the processor when the task is active.
         */
        double utilization;

        /**
         * @brief The processor on which the task is executed.
         */
        std::shared_ptr<processor> attached_proc{};

        /**
         * @brief A constructor with a unique ID, the period, and the utilization.
         * @param sim Weak pointer to the engine.
         * @param tid A unique ID.
         * @param period The period of the task.
         * @param utilization The utilization taken when active.
         */
        task(
            const std::weak_ptr<engine>& sim,
            std::size_t tid,
            const double& period,
            const double& utilization);

        /**
         * @brief Returns true if the task is currently attached to a processor.
         */
        [[nodiscard]] auto is_attached() const -> bool;

        /**
         * @brief Returns true if the task has remaining time to be executed.
         */
        [[nodiscard]] auto has_remaining_time() const -> bool;

        /**
         * @brief Adds a new job to the queue.
         * @param duration The duration of the new job.
         */
        void add_job(const double& duration);

        /**
         * @brief Consumes time from the remaining execution time.
         * @param duration The duration to consume.
         */
        void consume_time(const double& duration);

        /**
         * @brief Returns the remaining execution time.
         */
        [[nodiscard]] auto get_remaining_time() const -> double
        {
                return remaining_execution_time;
        };

        /**
         * @brief Returns true if the task has a pending job.
         */
        [[nodiscard]] auto has_job() const -> bool { return !pending_jobs.empty(); };

        /**
         * @brief Moves to the next job in the queue.
         */
        void next_job();

        /**
         * @brief Returns the server to which the task is attached.
         */
        [[nodiscard]] auto get_server() const -> std::shared_ptr<server> { return attached_serv; };

        /**
         * @brief Sets the server to which the task is attached.
         * @param serv_to_attach The server to attach the task to.
         */
        void set_server(const std::shared_ptr<server>& serv_to_attach);

        /**
         * @brief Unsets the server from the task.
         */
        void unset_server();

        /**
         * @brief Returns true if the task is attached to a server.
         */
        [[nodiscard]] auto has_server() const -> bool;

      private:
        /**
         * @brief Remaining execution time that the processor has to execute.
         * @description When a job arrives in the system, the value of this variable is increased by
         * the duration of the job that arrived.
         */
        double remaining_execution_time{0};

        /**
         * @brief Queue of worst-case execution times (WCET) of the pending jobs.
         * @description When a job arrives, its WCET is stored in the queue. When the task finishes
         * a job, the remaining_execution_time is set to the WCET of the next job.
         */
        std::queue<double> pending_jobs;

        /**
         * @brief The server to which the task is attached.
         */
        std::shared_ptr<server> attached_serv{};
};

#endif
