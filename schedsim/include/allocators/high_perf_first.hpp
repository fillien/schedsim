#ifndef ALLOCATOR_HIGH_PERF_FIRST
#define ALLOCATOR_HIGH_PERF_FIRST

#include <meta_scheduler.hpp>

namespace allocators {
class high_perf_first : public meta_scheduler {
      protected:
        auto where_to_put_the_task(const std::shared_ptr<task>& new_task)
            -> std::pair<std::shared_ptr<scheduler>, bool> final;

      public:
        explicit high_perf_first(const std::weak_ptr<engine> sim) : meta_scheduler(sim){};
};
}; // namespace allocators

#endif
