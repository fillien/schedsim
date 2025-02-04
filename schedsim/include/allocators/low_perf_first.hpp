#ifndef ALLOCATOR_LOW_PERF_FIRST
#define ALLOCATOR_LOW_PERF_FIRST

#include <meta_scheduler.hpp>

namespace allocators {
class low_perf_first : public meta_scheduler {
      protected:
        auto where_to_put_the_task(const std::shared_ptr<task>& new_task)
            -> std::pair<std::shared_ptr<scheds::scheduler>, bool> final;

      public:
        explicit low_perf_first(const std::weak_ptr<engine> sim) : meta_scheduler(sim){};
};
}; // namespace allocators

#endif
