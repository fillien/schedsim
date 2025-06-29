#ifndef ALLOCATOR_FF_BIG_FIRST
#define ALLOCATOR_FF_BIG_FIRST

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {

/**
 * @brief A class representing a high-performance first allocator.
 *
 * This allocator prioritizes scheduling tasks on schedulers with the highest performance.
 */
class FFBigFirst : public Allocator {
      protected:
        /**
         * @brief Determines the scheduler to place a new task onto, prioritizing those with higher
         * performance.
         *
         * @param new_task The task to be scheduled.
         * @return An optional containing the selected scheduler if one is found; otherwise,
         * std::nullopt.
         */
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> override;

      public:
        /**
         * @brief Constructor for the HighPerfFirst allocator.
         *
         * @param sim A weak pointer to the simulation engine.
         */
        explicit FFBigFirst(const std::weak_ptr<Engine>& sim) : Allocator(sim) {};
};

} // namespace allocators

#endif
