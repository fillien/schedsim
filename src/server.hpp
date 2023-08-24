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

        /// The task to ensure time isolation
        std::weak_ptr<task> attached_task;

        explicit server(const std::weak_ptr<engine> sim, const std::weak_ptr<task> attached_task);

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

        auto remaining_exec_time() {
                assert(!attached_task.expired());
                return attached_task.lock()->remaining_execution_time;
        }

        void change_state(const state& new_state);

        auto get_budget() -> double;

        void update_times(const double& timeframe);

        void postpone();
};

auto operator<<(std::ostream& out, const server& serv) -> std::ostream&;
auto operator<<(std::ostream& out, const server::state& s) -> std::ostream&;

#endif
