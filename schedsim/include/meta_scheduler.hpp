#ifndef META_SCHEDULER_HPP
#define META_SCHEDULER_HPP

#include "entity.hpp"
#include "event.hpp"
#include "scheduler.hpp"
#include <cstddef>
#include <memory>
#include <set>

class meta_scheduler : public entity {
      private:
        std::vector<std::shared_ptr<scheduler>> schedulers;

        std::set<std::shared_ptr<scheduler>> rescheds;

      public:
        explicit meta_scheduler(const std::weak_ptr<engine> sim) : entity(sim){};

        void add_child_sched(const std::weak_ptr<cluster>& clu);
        auto where_to_put_the_task(const std::shared_ptr<task>& new_task)
            -> std::pair<std::size_t, bool>;
        void handle(std::vector<events::event> evts);
        void call_resched(const std::shared_ptr<scheduler>& index) { rescheds.insert(index); };
};

#endif
