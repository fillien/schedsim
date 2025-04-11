#ifndef ALLOCATOR_LOW_PERF_FIRST
#define ALLOCATOR_LOW_PERF_FIRST

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {

/**
 * @brief A class representing a low-performance first allocator.
 *
 * This allocator prioritizes scheduling tasks on schedulers with lower performance metrics.
 */
class LowPerfFirst : public Allocator {
      protected:
        /**
         * @brief Determines the scheduler to place a new task onto, prioritizing those with lower
         * performance.
         *
         * This method iterates through available schedulers and selects the one with the lowest
         * performance score. If no suitable scheduler is found, it returns std::nullopt.
         *
         * @param new_task The task to be scheduled.
         * @return A std::optional containing a shared pointer to the selected scheduler, or
         * std::nullopt if no scheduler is available.
         */
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>>;

      public:
        /**
         * @brief Constructor for the LowPerfFirst allocator.
         *
         * Initializes the allocator with a weak pointer to the simulation engine.
         *
         * @param sim A weak pointer to the simulation engine.
         */
        explicit LowPerfFirst(const std::weak_ptr<Engine>& sim) : Allocator(sim) {};
};
}; // namespace allocators

#endif