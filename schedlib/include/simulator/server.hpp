#ifndef SERVER_HPP
#define SERVER_HPP

#include <simulator/entity.hpp>
#include <simulator/scheduler.hpp>
#include <simulator/task.hpp>

#include <cassert>
#include <cstddef>
#include <memory>

class Engine;
class Processor;

/**
 * @brief An entity attached to a task that ensures resources are reserved for this task.
 */
class Server : public Entity, public std::enable_shared_from_this<Server> {
      public:
        /**
         * @brief Possible states of a server.
         */
        enum class State { Inactive, Ready, Running, NonCont };

        /**
         * @brief Constructs a server with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         * @param sched Shared pointer to the scheduler.
         */
        explicit Server(
            const std::weak_ptr<Engine>& sim, const std::shared_ptr<scheds::Scheduler>& sched);

        /**
         * @brief Retrieves the ID of the attached task.
         * @return ID of the attached task.
         *
         * @pre A task must be attached.
         */
        [[nodiscard]] auto id() const noexcept -> std::size_t
        {
                auto t = task();
                assert(t && "No task attached to server");
                return t->id();
        }

        /**
         * @brief Retrieves the utilization of the attached task.
         * @return Utilization of the attached task.
         *
         * @pre A task must be attached.
         */
        [[nodiscard]] auto utilization() const noexcept -> double
        {
                auto t = task();
                assert(t && "No task attached to server");
                return t->utilization();
        }

        /**
         * @brief Retrieves the period of the attached task.
         * @return Period of the attached task.
         *
         * @pre A task must be attached.
         */
        [[nodiscard]] auto period() const noexcept -> double
        {
                auto t = task();
                assert(t && "No task attached to server");
                return t->period();
        }

        /**
         * @brief Changes the state of the server.
         * @param new_state New state to set.
         */
        auto change_state(State new_state) -> void;

        /**
         * @brief Retrieves the current state of the server.
         * @return The current state.
         */
        [[nodiscard]] auto state() const noexcept -> State { return current_state_; }

        /**
         * @brief Retrieves the relative deadline for the attached task.
         * @return The relative deadline.
         */
        [[nodiscard]] auto deadline() const noexcept -> double { return relative_deadline_; }

        /**
         * @brief Retrieves the current virtual time.
         * @return The virtual time.
         */
        [[nodiscard]] auto virtual_time() const noexcept -> double { return virtual_time_; }

        /**
         * @brief Updates the virtual time.
         * @param new_time The new virtual time.
         *
         * @pre new_time must be greater than or equal to the current virtual time.
         */
        auto virtual_time(double new_time) -> void
        {
                assert(
                    new_time >= virtual_time_ &&
                    "New virtual time must be no less than the current virtual time");
                virtual_time_ = new_time;
        }

        /**
         * @brief Retrieves the running time of the server.
         * @return The running time.
         */
        [[nodiscard]] auto running_time() const -> double;

        /**
         * @brief Updates the server's internal time.
         */
        auto update_time() -> void;

        /**
         * @brief Retrieves the budget of the attached task.
         * @return The budget of the attached task.
         */
        [[nodiscard]] auto budget() -> double;

        /**
         * @brief Postpones the attached task.
         */
        auto postpone() -> void;

        /**
         * @brief Retrieves the attached task.
         * @return Shared pointer to the attached task.
         */
        [[nodiscard]] auto task() const -> std::shared_ptr<Task> { return attached_task_.lock(); }

        /**
         * @brief Attaches a task to the server.
         * @param task_to_attach Task to attach to the server.
         */
        auto attach_task(const std::shared_ptr<Task>& task_to_attach) -> void
        {
                attached_task_ = task_to_attach;
        }

        /**
         * @brief Detaches the currently attached task.
         */
        auto detach_task() -> void { attached_task_.reset(); }

        /**
         * @brief Indicates whether the server cannot be inactive.
         * @return True if the server cannot be inactive, false otherwise.
         */
        [[nodiscard]] auto cant_be_inactive() const noexcept -> bool { return cant_be_inactive_; }

        /**
         * @brief Checks if a task is attached to the server.
         * @return True if a task is attached, false otherwise.
         */
        [[nodiscard]] auto has_task() const noexcept -> bool { return !attached_task_.expired(); }

        [[nodiscard]] auto scheduler() const noexcept -> const std::shared_ptr<scheds::Scheduler>
        {
                return attached_sched_.lock();
        }

        /**
         * @brief Default destructor.
         */
        ~Server() = default;

        /**
         * @brief Copy constructor.
         * @param other The server to copy from.
         */
        Server(const Server& other) = default;

        /**
         * @brief Move constructor.
         * @param other The server to move from.
         */
        Server(Server&& other) noexcept = default;

        /**
         * @brief Copy assignment operator.
         * @param other The server to copy from.
         * @return A reference to the current object.
         */
        auto operator=(const Server& other) -> Server& = default;

        /**
         * @brief Move assignment operator.
         * @param other The server to move from.
         * @return A reference to the current object.
         */
        auto operator=(Server&& other) noexcept -> Server& = default;

      private:
        std::weak_ptr<Task> attached_task_; ///< The task to ensure time isolation.
        std::weak_ptr<scheds::Scheduler> attached_sched_;
        State current_state_{State::Inactive}; ///< Current state of the server.
        double relative_deadline_{0.0};        ///< Relative deadline for the attached task.
        double virtual_time_{0.0};             ///< Virtual time for the attached task.
        bool cant_be_inactive_{
            false};               ///< Flag indicating if the server cannot be in an inactive state.
        double last_call_{0.0};   ///< Timestamp of the last call made by the server.
        double last_update_{0.0}; ///< Timestamp of the last update made by the server.
};

#endif // SERVER_HPP