#ifndef ALLOCATOR_FF_CAP
#define ALLOCATOR_FF_CAP

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {
/**
 * @brief First-Fit allocator with per-cluster capacity cap.
 *
 * @details Clusters are iterated in increasing `perf()`. For each cluster,
 * the task's scaled utilization is checked against the cluster's target
 * utilization `u_target()`. The first cluster whose scheduler both meets the
 * cap and passes `admission_test()` is selected.
 */
class FFCap : public Allocator {
      protected:
        /**
         * @brief Choose the first cluster that satisfies the capacity cap.
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
        explicit FFCap(const std::weak_ptr<Engine>& sim) : Allocator(sim) {};
};
}; // namespace allocators

#endif
