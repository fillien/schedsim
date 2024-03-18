#ifndef SERVER_HPP
#define SERVER_HPP

#include "entity.hpp"
#include "task.hpp"
#include <cstddef>
#include <memory>
#include <ostream>

class processor;

/**
 * @brief An entity attached to a task that ensures resources are reserved for this task.
 */
class server : public entity, public std::enable_shared_from_this<server> {
      private:
        /**
         * @brief The task to ensure time isolation.
         */
        std::weak_ptr<task> attached_task{};

      public:
        /**
         * @brief Possible states of a server.
         */
        enum class state { inactive, ready, running, non_cont };

        /**
         * @brief Current state of the server.
         */
        state current_state{state::inactive}; /**< The current state of the server. */

        double relative_deadline{0}; /**< Relative deadline for the attached task. */
        double virtual_time{0};      /**< Virtual time for the attached task. */

        bool cant_be_inactive{
            false};            /**< Flag indicating if the server cannot be in an inactive state. */
        double last_call{0};   /**< Timestamp of the last call made by the server. */
        double last_update{0}; /**< Timestamp of the last update made by the server. */

        /**
         * @brief Constructs a server with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit server(const std::weak_ptr<engine>& sim);

        /**
         * @brief Retrieves the ID of the attached task.
         * @return ID of the attached task.
         */
        auto id() const -> std::size_t { return get_task()->id; }

        /**
         * @brief Retrieves the utilization of the attached task.
         * @return Utilization of the attached task.
         */
        auto utilization() const -> double { return get_task()->utilization; };

        /**
         * @brief Retrieves the period of the attached task.
         * @return Period of the attached task.
         */
        auto period() const -> double { return get_task()->period; };

        /**
         * @brief Retrieves the remaining execution time of the attached task.
         * @return Remaining execution time of the attached task.
         */
        auto remaining_exec_time() const -> double;

        /**
         * @brief Changes the state of the server.
         * @param new_state New state to set.
         */
        void change_state(const state& new_state);

        /**
         * @brief Retrieves the budget of the attached task.
         * @return Budget of the attached task.
         */
        auto budget() -> double;

        /**
         * @brief Postpones the attached task.
         */
        void postpone();

        /**
         * @brief Retrieves the attached task.
         * @return Shared pointer to the attached task.
         */
        auto get_task() const -> std::shared_ptr<task> { return attached_task.lock(); };

        /**
         * @brief Sets the attached task.
         * @param task_to_attach Task to attach to the server.
         */
        void set_task(const std::shared_ptr<task>& task_to_attach);

        /**
         * @brief Unsets the attached task.
         */
        void unset_task();

        /**
         * @brief Checks if the server has an attached task.
         * @return True if the server has an attached task, false otherwise.
         */
        auto has_task() const -> bool { return attached_task.use_count() > 0; };
};

/**
 * @brief Overloaded stream insertion operator for the server class.
 * @param out Output stream.
 * @param serv Server instance to output.
 * @return Reference to the output stream.
 */
auto operator<<(std::ostream& out, const server& serv) -> std::ostream&;

/**
 * @brief Overloaded stream insertion operator for the server::state enumeration.
 * @param out Output stream.
 * @param serv_state Server state enumeration value.
 * @return Reference to the output stream.
 */
auto operator<<(std::ostream& out, const server::state& serv_state) -> std::ostream&;

#endif // SERVER_HPP
