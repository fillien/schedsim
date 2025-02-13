#ifndef META_SCHEDULER_HPP
#define META_SCHEDULER_HPP

#include <cstddef>
#include <entity.hpp>
#include <event.hpp>
#include <memory>
#include <optional>
#include <scheduler.hpp>
#include <set>

namespace allocators {
class allocator : public Entity {
      private:
        std::set<std::shared_ptr<scheds::Scheduler>> rescheds;

      protected:
        std::vector<std::shared_ptr<scheds::Scheduler>> schedulers;
        virtual auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> = 0;

      public:
        explicit allocator(const std::weak_ptr<engine>& sim) : Entity(sim){};
        virtual ~allocator() = default;

        void add_child_sched(const std::weak_ptr<Cluster>& clu);
        void handle(std::vector<events::event> evts);
        void call_resched(const std::shared_ptr<scheds::Scheduler>& index)
        {
                rescheds.insert(index);
        };
};
} // namespace allocators
#endif
