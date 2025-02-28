#ifndef META_SCHEDULER_HPP
#define META_SCHEDULER_HPP

#include <entity.hpp>
#include <event.hpp>
#include <memory>
#include <optional>
#include <scheduler.hpp>
#include <set>
#include <vector>

namespace allocators {

/**
 * @brief Base class for scheduling allocation.
 */
class Allocator : public Entity {
      public:
        explicit Allocator(const std::weak_ptr<Engine>& sim) : Entity(sim) {}
        virtual ~Allocator() = default;
        Allocator(const Allocator&) = delete;
        auto operator=(const Allocator&) -> Allocator& = delete;
        Allocator(Allocator&&) noexcept = delete;
        auto operator=(Allocator&&) noexcept -> Allocator& = delete;

        auto add_child_sched(
            const std::weak_ptr<Cluster>& clu, const std::shared_ptr<scheds::Scheduler>& sched)
            -> void;
        auto handle(std::vector<events::Event> evts) -> void;
        auto call_resched(const std::shared_ptr<scheds::Scheduler>& index) -> void
        {
                rescheds_.insert(index);
        }

      protected:
        virtual auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> = 0;

        [[nodiscard]] auto schedulers() const
            -> const std::vector<std::shared_ptr<scheds::Scheduler>>&
        {
                return schedulers_;
        }

      private:
        static auto migrate_task(
            const events::JobArrival& evt, const std::shared_ptr<scheds::Scheduler>& receiver)
            -> void;

        std::vector<std::shared_ptr<scheds::Scheduler>> schedulers_;
        std::set<std::shared_ptr<scheds::Scheduler>> rescheds_;
};

} // namespace allocators

#endif // META_SCHEDULER_HPP
