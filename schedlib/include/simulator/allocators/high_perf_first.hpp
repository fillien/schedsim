#ifndef ALLOCATOR_HIGH_PERF_FIRST
#define ALLOCATOR_HIGH_PERF_FIRST

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {
class HighPerfFirst : public Allocator {
      protected:
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>>;

      public:
        explicit HighPerfFirst(const std::weak_ptr<Engine>& sim) : Allocator(sim) {};
};
}; // namespace allocators

#endif
