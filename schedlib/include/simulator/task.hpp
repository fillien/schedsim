#ifndef TASK_HPP
#define TASK_HPP

#include <simulator/entity.hpp>

#include <cstddef>
#include <queue>

class Processor;
class Server;
class Engine;

/**
 * @brief Represents a model of user code executed by a processor.
 */
class Task : public Entity {
      public:
        /**
         * @brief Constructs a Task.
         * @param engine Reference to the engine.
         * @param id Unique identifier for the task.
         * @param period Task period.
         * @param utilization Processor utilization when active.
         */
        Task(Engine& engine, std::size_t tid, double period, double utilization);

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

        [[nodiscard]] auto has_remaining_time() const noexcept -> bool;
        auto add_job(double duration) -> void;
        auto consume_time(double duration) -> void;
        [[nodiscard]] auto remaining_time() const noexcept -> double;
        [[nodiscard]] auto has_job() const noexcept -> bool { return !pending_jobs_.empty(); }
        auto next_job() -> void;

        [[nodiscard]] auto server() const noexcept -> Server* { return attached_serv_; }
        auto server(Server* serv_to_attach) -> void;
        auto clear_server() -> void;

        [[nodiscard]] auto has_server() const noexcept -> bool
        {
                return attached_serv_ != nullptr;
        }

        [[nodiscard]] auto id() const noexcept -> std::size_t { return id_; }
        [[nodiscard]] auto utilization() const noexcept -> double { return utilization_; }
        [[nodiscard]] auto period() const noexcept -> double { return period_; }

        [[nodiscard]] auto proc() const noexcept -> Processor* { return attached_proc_; }
        auto proc(Processor* receiver) -> void { attached_proc_ = receiver; }

      private:
        std::size_t id_{0};
        double period_{0.0};
        double utilization_{0.0};
        double remaining_execution_time_{0.0};

        std::queue<double> pending_jobs_;

        Processor* attached_proc_{nullptr};
        Server* attached_serv_{nullptr};
};

#endif // TASK_HPP
