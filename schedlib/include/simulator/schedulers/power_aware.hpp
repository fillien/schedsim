#ifndef SCHED_POWER_AWARE_HPP
#define SCHED_POWER_AWARE_HPP

#include <memory>
#include <simulator/entity.hpp>
#include <simulator/schedulers/parallel.hpp>

namespace scheds {
class PowerAware : public Parallel {
      protected:
        /**
         * @brief Returns the number of active processors.
         * @return The number of active processors.
         */
        auto nb_active_procs() const -> std::size_t;

      public:
        /**
         * @brief Constructs a parallel scheduler with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit PowerAware(const std::weak_ptr<Engine>& sim) : Parallel(sim) {};

        /**
         * @brief Updates the platform state based on power awareness considerations.
         * This function overrides the base class's update_platform method.
         */
        void update_platform() override;
};
} // namespace scheds
#endif