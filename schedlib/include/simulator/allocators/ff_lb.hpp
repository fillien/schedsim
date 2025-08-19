#ifndef ALLOCATOR_FIRST_FIT_LOAD_BALANCER
#define ALLOCATOR_FIRST_FIT_LOAD_BALANCER

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {
/**
 * @brief First-Fit allocator with load-balancing bias.
 *
 * @details Chooses the first scheduler that passes `admission_test()` while
 * following a policy that encourages load balancing across clusters.
 */
class FirstFitLoadBalancer : public Allocator {
      protected:
        /**
         * @brief Select a destination aiming to balance load.
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
        explicit FirstFitLoadBalancer(const std::weak_ptr<Engine>& sim) : Allocator(sim) {};
};
}; // namespace allocators

#endif
