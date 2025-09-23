#ifndef ALLOCATOR_FF_SMA
#define ALLOCATOR_FF_SMA

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {
/**
 * @brief First-Fit allocator guided by a simple moving average.
 *
 * @details Uses a simple moving average of recent utilization on the largest
 * cluster to adjust `u_target()` on other clusters, then applies a capacity
 * check and first-fit admission.
 */
class FFSma : public Allocator {
      protected:
        /**
         * @brief Choose a cluster based on SMA-adjusted capacity targets.
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
         * @param sample_rate Number of samples collected per unit time for the SMA window.
         * @param num_samples Number of samples included in the SMA window.
         */
        explicit FFSma(
            const std::weak_ptr<Engine>& sim, double sample_rate = 0.5, int num_samples = 5);

      private:
        double sample_rate_;
        int num_samples_;
};
}; // namespace allocators

#endif
