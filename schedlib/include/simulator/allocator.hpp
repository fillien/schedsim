#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include <simulator/event.hpp>
#include <simulator/scheduler.hpp>

#include <memory>
#include <set>
#include <vector>

class Engine;

namespace allocators {

/**
 * @brief Base class coordinating task placement across clusters.
 */
class Allocator : public Entity {
      public:
        explicit Allocator(Engine& sim) : Entity(sim) {}
        virtual ~Allocator() = default;
        Allocator(const Allocator&) = delete;
        auto operator=(const Allocator&) -> Allocator& = delete;
        Allocator(Allocator&&) noexcept = delete;
        auto operator=(Allocator&&) noexcept -> Allocator& = delete;

        auto add_child_sched(Cluster* clu, std::unique_ptr<scheds::Scheduler> sched) -> void;

        auto handle(std::vector<events::Event> evts) -> void;

        auto call_resched(scheds::Scheduler* index) -> void { rescheds_.insert(index); }

      protected:
        /**
         * @brief Decide the destination scheduler for a newly arrived job.
         *
         * @param new_task Task to be placed.
         * @return Selected scheduler, or nullptr if no placement is possible.
         */
        virtual auto where_to_put_the_task(const Task& new_task) -> scheds::Scheduler* = 0;

        [[nodiscard]] auto schedulers() const
            -> const std::vector<std::unique_ptr<scheds::Scheduler>>&
        {
                return schedulers_;
        }

      private:
        static auto migrate_task(
            const events::JobArrival& evt, scheds::Scheduler* receiver) -> void;

        auto need_to_place_task(const auto& new_job) -> void;

        std::vector<std::unique_ptr<scheds::Scheduler>> schedulers_;
        std::set<scheds::Scheduler*> rescheds_;
};

} // namespace allocators

#endif // ALLOCATOR_HPP
