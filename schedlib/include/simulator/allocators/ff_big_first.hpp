#ifndef ALLOCATOR_FF_BIG_FIRST
#define ALLOCATOR_FF_BIG_FIRST

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {

/**
 * @brief First-Fit allocator preferring highest-performance clusters.
 *
 * @details Schedulers (clusters) are sorted by decreasing `perf()`. The first
 * scheduler that accepts the task via `admission_test()` is selected.
 */
class FFBigFirst : public Allocator {
      protected:
        /**
         * @brief Pick the highest-performance scheduler that can admit the task.
         *
         * @param new_task Task to be scheduled.
         * @return Selected scheduler, or std::nullopt if none can admit it.
         */
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> override;

      public:
        /**
         * @brief Construct the allocator.
         * @param sim Weak pointer to the simulation engine.
         */
        explicit FFBigFirst(const std::weak_ptr<Engine>& sim) : Allocator(sim) {};
};

} // namespace allocators

#endif
