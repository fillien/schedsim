#ifndef ALLOCATOR_FF_CAP_ADAPTIVE_LINEAR
#define ALLOCATOR_FF_CAP_ADAPTIVE_LINEAR

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {

/**
 * @brief First-Fit allocator with adaptive LITTLE-cluster capacity using a linear model.
 *
 * Computes the utilization threshold as: target = A*umax + B*U + C
 * where umax is the maximum observed task utilization and U is the expected total system utilization.
 */
class FFCapAdaptiveLinear : public Allocator {
      private:
        double observed_umax_ = 0.0;
        double expected_total_util_ = 0.0;

      protected:
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> override;

      public:
        explicit FFCapAdaptiveLinear(const std::weak_ptr<Engine>& sim) : Allocator(sim) {}

        void set_expected_total_util(double util) { expected_total_util_ = util; }
};

} // namespace allocators

#endif
