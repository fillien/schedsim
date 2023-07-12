#ifndef SERVER_HPP
#define SERVER_HPP

#include "entity.hpp"
#include "task.hpp"

#include <cassert>
#include <memory>
#include <vector>

class processor;

/// An entity attached to a task that ensure ressources are reserved for this task.
class server : public entity {
      public:
        /// Possible states of a server
        enum class state { idle, running, active, active_non_contending };

        /// Current state of the server
        state current_state{state::idle};

        double relative_deadline{0};
        double virtual_time{0};

        /// The task to ensure time isolation
        std::weak_ptr<task> attached_task;

        explicit server(const std::weak_ptr<task> attached_task);
        auto id() const -> int {
                assert(!attached_task.expired());
                return attached_task.lock()->id;
        }
        auto utilization() const -> double {
                assert(!attached_task.expired());
                return attached_task.lock()->utilization;
        };
        auto period() const -> double {
                assert(!attached_task.expired());
                return attached_task.lock()->period;
        };
};

auto operator<<(std::ostream& out, const server& serv) -> std::ostream&;

#endif
