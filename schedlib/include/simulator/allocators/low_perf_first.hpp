#ifndef ALLOCATOR_LOW_PERF_FIRST
#define ALLOCATOR_LOW_PERF_FIRST

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {
class LowPerfFirst : public Allocator {
      protected:
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>>;

      public:
        explicit LowPerfFirst(const std::weak_ptr<Engine>& sim) : Allocator(sim) {};
};
}; // namespace allocators

#endif
