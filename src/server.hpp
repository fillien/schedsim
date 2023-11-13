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
      private:
        /// The task to ensure time isolation
        std::weak_ptr<task> attached_task{};

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

        explicit server(const std::weak_ptr<engine>& sim);

        auto id() const -> int { return get_task()->id; }

        auto utilization() const -> double { return get_task()->utilization; };

        auto period() const -> double { return get_task()->period; };

        auto remaining_exec_time() const -> double { return get_task()->get_remaining_time(); }

        void change_state(const state& new_state);

        auto get_budget() -> double;

        void postpone();

        auto get_task() const -> std::shared_ptr<task> { return attached_task.lock(); };
        void set_task(const std::shared_ptr<task>& task_to_attach);
        void unset_task();
        auto has_task() const -> bool { return attached_task.use_count() > 0; };
};

auto operator<<(std::ostream& out, const server& serv) -> std::ostream&;
auto operator<<(std::ostream& out, const server::state& serv_state) -> std::ostream&;

#endif
