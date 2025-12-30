#ifndef ALLOCATOR_FF_CAP_ADAPTIVE_POLY
#define ALLOCATOR_FF_CAP_ADAPTIVE_POLY

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {

/**
 * @brief First-Fit allocator with adaptive LITTLE-cluster capacity using a polynomial model.
 *
 * Computes the utilization threshold as:
 *   target = C0 + C1*umax + C2*U + C3*umax² + C4*umax*U + C5*U²
 * where umax is the maximum observed task utilization and U is the expected total system utilization.
 */
class FFCapAdaptivePoly : public Allocator {
      private:
        double observed_umax_ = 0.0;
        double expected_total_util_ = 0.0;

      protected:
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> override;

      public:
        explicit FFCapAdaptivePoly(const std::weak_ptr<Engine>& sim) : Allocator(sim) {}

        void set_expected_total_util(double util) { expected_total_util_ = util; }
};

} // namespace allocators

#endif
