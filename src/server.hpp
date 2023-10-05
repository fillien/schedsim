#ifndef SERVER_HPP
#define SERVER_HPP

#include "entity.hpp"
#include "task.hpp"

#include <cassert>
#include <memory>
#include <vector>

class processor;

/// An entity attached to a task that ensure ressources are reserved for this task.
class server : public entity, public std::enable_shared_from_this<server> {
      public:
        /// Possible states of a server
        enum class state { inactive, ready, running, non_cont };

        /// Current state of the server
        state current_state{state::inactive};

        double relative_deadline{0};
        double virtual_time{0};

        bool cant_be_inactive{false};
        double last_call{0};
        double last_update{0};

        /// The task to ensure time isolation
        std::shared_ptr<task> attached_task;

        explicit server(const std::weak_ptr<engine> sim, const std::weak_ptr<task> attached_task);

        auto id() const -> int { return attached_task->id; }

        auto utilization() const -> double { return attached_task->utilization; };

        auto period() const -> double { return attached_task->period; };

        auto remaining_exec_time() { return attached_task->get_remaining_time(); }

        void change_state(const state& new_state);

        auto get_budget() -> double;

        void postpone();
};

auto operator<<(std::ostream& out, const server& serv) -> std::ostream&;
auto operator<<(std::ostream& out, const server::state& s) -> std::ostream&;

#endif
