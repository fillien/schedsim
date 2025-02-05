#ifndef META_SCHEDULER_HPP
#define META_SCHEDULER_HPP

#include <cstddef>
#include <entity.hpp>
#include <event.hpp>
#include <memory>
#include <scheduler.hpp>
#include <set>

namespace allocators {
class allocator : public entity {
      private:
        std::set<std::shared_ptr<scheds::scheduler>> rescheds;

      protected:
        std::vector<std::shared_ptr<scheds::scheduler>> schedulers;
        virtual auto where_to_put_the_task(const std::shared_ptr<task>& new_task)
            -> std::pair<std::shared_ptr<scheds::scheduler>, bool> = 0;

      public:
        explicit allocator(const std::weak_ptr<engine>& sim) : entity(sim){};
        virtual ~allocator() = default;

        void add_child_sched(const std::weak_ptr<cluster>& clu);
        void handle(std::vector<events::event> evts);
        void call_resched(const std::shared_ptr<scheds::scheduler>& index)
        {
                rescheds.insert(index);
        };
};
} // namespace allocators
#endif
