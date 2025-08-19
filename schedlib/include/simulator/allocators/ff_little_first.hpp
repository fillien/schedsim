#ifndef ALLOCATOR_FF_LITTLE_FIRST
#define ALLOCATOR_FF_LITTLE_FIRST

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {

/**
 * @brief First-Fit allocator preferring lowest-performance clusters.
 *
 * @details Schedulers (clusters) are sorted by increasing `perf()` and the
 * first that admits the task is chosen. Useful to bias load toward slower
 * clusters first, preserving faster ones for future load.
 */
class FFLittleFirst : public Allocator {
      protected:
        /**
         * @brief Pick the lowest-performance scheduler that can admit the task.
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
        explicit FFLittleFirst(const std::weak_ptr<Engine>& sim) : Allocator(sim) {};
};
}; // namespace allocators

#endif
