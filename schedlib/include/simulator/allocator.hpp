#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include <simulator/event.hpp>
#include <simulator/scheduler.hpp>

#include <memory>
#include <optional>
#include <set>
#include <vector>

class Engine;

namespace allocators {

/**
 * @brief Base class for scheduling allocation.
 */
class Allocator : public Entity {
      public:
        /**
         * @brief Constructor for the Allocator class.
         *
         * @param sim A weak pointer to the simulation engine.
         */
        explicit Allocator(const std::weak_ptr<Engine>& sim) : Entity(sim) {}
        virtual ~Allocator() = default;
        Allocator(const Allocator&) = delete;
        auto operator=(const Allocator&) -> Allocator& = delete;
        Allocator(Allocator&&) noexcept = delete;
        auto operator=(Allocator&&) noexcept -> Allocator& = delete;

        /**
         * @brief Adds a child scheduler to the allocator.
         *
         * @param clu A weak pointer to the cluster.
         * @param sched A shared pointer to the scheduler.
         */
        auto add_child_sched(
            const std::weak_ptr<Cluster>& clu, const std::shared_ptr<scheds::Scheduler>& sched)
            -> void;

        /**
         * @brief Handles a vector of events.
         *
         * @param evts A vector of events to handle.
         */
        auto handle(std::vector<events::Event> evts) -> void;

        /**
         * @brief Marks a scheduler for rescheduling.
         *
         * @param index A shared pointer to the scheduler to reschedule.
         */
        auto call_resched(const std::shared_ptr<scheds::Scheduler>& index) -> void
        {
                rescheds_.insert(index);
        }

      protected:
        /**
         * @brief Determines where to place a new task.
         *
         * @param new_task A shared pointer to the task to be placed.
         * @return An optional shared pointer to the scheduler where the task should be placed, or
         * std::nullopt if no suitable scheduler is found.
         */
        virtual auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> = 0;

        /**
         * @brief Returns the list of schedulers managed by this allocator.
         *
         * @return A const reference to a vector of shared pointers to schedulers.
         */
        [[nodiscard]] auto schedulers() const
            -> const std::vector<std::shared_ptr<scheds::Scheduler>>&
        {
                return schedulers_;
        }

      private:
        /**
         * @brief Migrates a task from one scheduler to another.
         *
         * @param evt The job arrival event triggering the migration.
         * @param receiver A shared pointer to the receiving scheduler.
         */
        static auto migrate_task(
            const events::JobArrival& evt, const std::shared_ptr<scheds::Scheduler>& receiver)
            -> void;

        std::vector<std::shared_ptr<scheds::Scheduler>> schedulers_;
        std::set<std::shared_ptr<scheds::Scheduler>> rescheds_;
};

} // namespace allocators

#endif // ALLOCATOR_HPP
