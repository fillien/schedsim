#ifndef SCHED_POWER_AWARE_HPP
#define SCHED_POWER_AWARE_HPP

#include <entity.hpp>
#include <memory>
#include <schedulers/parallel.hpp>

namespace scheds {
class power_aware : public parallel {
      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        /**
         * @brief Constructs a parallel scheduler with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit power_aware(const std::weak_ptr<engine>& sim) : parallel(sim){};

        void update_platform() override;
};
} // namespace scheds
#endif
