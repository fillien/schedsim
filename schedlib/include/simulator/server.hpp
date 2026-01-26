#ifndef SERVER_HPP
#define SERVER_HPP

#include <simulator/entity.hpp>
#include <simulator/task.hpp>

#include <cassert>
#include <cstddef>

class Engine;
class Processor;

namespace scheds {
class Scheduler;
}

/**
 * @brief An entity attached to a task that ensures resources are reserved for this task.
 */
class Server : public Entity {
      public:
        enum class State { Inactive, Ready, Running, NonCont };

        /**
         * @brief Constructs a server with a reference to the engine.
         * @param sim Reference to the engine.
         * @param sched Pointer to the scheduler.
         */
        explicit Server(Engine& sim, scheds::Scheduler* sched);

        [[nodiscard]] auto id() const noexcept -> std::size_t
        {
                auto* t = task();
                assert(t && "No task attached to server");
                return t->id();
        }

        [[nodiscard]] auto utilization() const noexcept -> double
        {
                auto* t = task();
                assert(t && "No task attached to server");
                return t->utilization();
        }

        [[nodiscard]] auto period() const noexcept -> double
        {
                auto* t = task();
                assert(t && "No task attached to server");
                return t->period();
        }

        auto change_state(State new_state) -> void;
        [[nodiscard]] auto state() const noexcept -> State { return current_state_; }
        [[nodiscard]] auto deadline() const noexcept -> double { return relative_deadline_; }
        [[nodiscard]] auto virtual_time() const noexcept -> double { return virtual_time_; }

        auto virtual_time(double new_time) -> void
        {
                assert(
                    new_time >= virtual_time_ &&
                    "New virtual time must be no less than the current virtual time");
                virtual_time_ = new_time;
        }

        [[nodiscard]] auto running_time() const -> double;
        auto update_time() -> void;
        [[nodiscard]] auto budget() -> double;
        auto postpone() -> void;

        [[nodiscard]] auto task() const -> Task* { return attached_task_; }
        auto attach_task(Task* task_to_attach) -> void { attached_task_ = task_to_attach; }
        auto detach_task() -> void { attached_task_ = nullptr; }

        [[nodiscard]] auto cant_be_inactive() const noexcept -> bool { return cant_be_inactive_; }
        [[nodiscard]] auto has_task() const noexcept -> bool { return attached_task_ != nullptr; }

        [[nodiscard]] auto scheduler() const noexcept -> scheds::Scheduler*
        {
                return attached_sched_;
        }

        bool been_migrated{false};

      private:
        Task* attached_task_{nullptr};
        scheds::Scheduler* attached_sched_;
        State current_state_{State::Inactive};
        double relative_deadline_{0.0};
        double virtual_time_{0.0};
        bool cant_be_inactive_{false};
        double last_call_{0.0};
        double last_update_{0.0};
};

#endif // SERVER_HPP
