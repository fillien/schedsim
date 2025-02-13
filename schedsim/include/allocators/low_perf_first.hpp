#ifndef ALLOCATOR_LOW_PERF_FIRST
#define ALLOCATOR_LOW_PERF_FIRST

#include <allocator.hpp>
#include <optional>

namespace allocators {
class low_perf_first : public allocator {
      protected:
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::scheduler>> final;

      public:
        explicit low_perf_first(const std::weak_ptr<engine>& sim) : allocator(sim){};
};
}; // namespace allocators

#endif
