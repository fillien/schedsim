#ifndef SCHED_POWER_AWARE_TIMER_HPP
#define SCHED_POWER_AWARE_TIMER_HPP

#include <entity.hpp>
#include <memory>
#include <schedulers/parallel.hpp>

namespace scheds {
class power_aware_timer : public Parallel {
      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        /**
         * @brief Constructs a parallel scheduler with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit power_aware_timer(const std::weak_ptr<engine>& sim);

        void update_platform() override;
};
} // namespace scheds

#endif
