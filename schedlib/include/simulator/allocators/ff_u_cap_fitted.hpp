#ifndef ALLOCATOR_FF_U_CAP_FITTED
#define ALLOCATOR_FF_U_CAP_FITTED

#include <optional>
#include <simulator/allocator.hpp>

namespace allocators {

/**
 * @brief First-Fit allocator that tunes LITTLE-cluster capacity with a fitted utilization model.
 */
class FFUCapFitted : public Allocator {
      protected:
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> override;

      public:
        explicit FFUCapFitted(const std::weak_ptr<Engine>& sim) : Allocator(sim) {}
};

} // namespace allocators

#endif
