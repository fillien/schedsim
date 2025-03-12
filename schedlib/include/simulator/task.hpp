#ifndef TASK_HPP
#define TASK_HPP

#include <simulator/entity.hpp>

#include <cstddef>
#include <memory>
#include <queue>

class Processor;
class Server;
class Engine;

/**
 * @brief Represents a model of user code executed by a processor.
 */
class Task : public Entity, public std::enable_shared_from_this<Task> {
      public:
        /**
         * @brief Constructs a Task.
         * @param engine Weak pointer to the engine.
         * @param id Unique identifier for the task.
         * @param period Task period.
         * @param utilization Processor utilization when active.
         */
        Task(std::weak_ptr<Engine> engine, std::size_t tid, double period, double utilization);

        // Prevent copying/moving.
        Task(const Task&) = delete;
        auto operator=(const Task&) -> Task& = delete;
        Task(Task&&) = delete;
        auto operator=(Task&&) -> Task& = delete;

        /**
         * @brief Checks if the task is attached to a processor.
         * @return true if attached; false otherwise.
         */
        [[nodiscard]] auto is_attached() const noexcept -> bool
        {
                return (attached_proc_ != nullptr);
        }

        /**
         * @brief Checks if the task has remaining execution time.
         * @return true if there is remaining execution time; false otherwise.
         */
        [[nodiscard]] auto has_remaining_time() const noexcept -> bool;

        /**
         * @brief Adds a new job with the specified duration.
         * @param duration Duration of the new job.
         */
        auto add_job(double duration) -> void;

        /**
         * @brief Consumes a specified duration from the remaining execution time.
         * @param duration Duration to consume.
         */
        auto consume_time(double duration) -> void;

        /**
         * @brief Returns the remaining execution time.
         * @return The remaining execution time.
         */
        [[nodiscard]] auto remaining_time() const noexcept -> double;

        /**
         * @brief Checks if there is a pending job.
         * @return true if there is at least one pending job; false otherwise.
         */
        [[nodiscard]] auto has_job() const noexcept -> bool { return !pending_jobs_.empty(); }

        /**
         * @brief Moves to the next job in the queue.
         */
        auto next_job() -> void;

        /**
         * @brief Retrieves the server the task is attached to.
         * @return Shared pointer to the attached server.
         */
        [[nodiscard]] auto server() const noexcept -> std::shared_ptr<Server>
        {
                return attached_serv_;
        }

        /**
         * @brief Attaches the task to a server.
         * @param serv_to_attach The server to attach to.
         */
        auto server(const std::shared_ptr<Server>& serv_to_attach) -> void;

        /**
         * @brief Detaches the server from the task.
         */
        auto clear_server() -> void;

        /**
         * @brief Checks if the task is attached to a server.
         * @return true if a server is attached; false otherwise.
         */
        [[nodiscard]] auto has_server() const noexcept -> bool
        {
                return static_cast<bool>(attached_serv_);
        }

        [[nodiscard]] auto id() const noexcept -> std::size_t { return id_; }

        [[nodiscard]] auto utilization() const noexcept -> double { return utilization_; }

        [[nodiscard]] auto period() const noexcept -> double { return period_; }

        [[nodiscard]] auto proc() const noexcept -> std::shared_ptr<Processor>
        {
                return attached_proc_;
        }

        auto proc(std::shared_ptr<Processor> receiver) -> void { attached_proc_ = receiver; }

      private:
        std::size_t id_{0};                    ///< Unique identifier for the task.
        double period_{0.0};                   ///< Task period.
        double utilization_{0.0};              ///< Processor utilization when active.
        double remaining_execution_time_{0.0}; ///< Remaining execution time.

        std::queue<double> pending_jobs_; ///< Queue of pending job durations (WCETs).

        std::shared_ptr<Processor> attached_proc_{
            nullptr};                                    ///< Processor on which the task executes.
        std::shared_ptr<Server> attached_serv_{nullptr}; ///< Server the task is attached to.
};

#endif // TASK_HPP
