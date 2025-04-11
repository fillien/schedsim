#ifndef SCHED_POWER_AWARE_TIMER_HPP
#define SCHED_POWER_AWARE_TIMER_HPP

#include <simulator/entity.hpp>
#include <simulator/schedulers/parallel.hpp>

#include <memory>

namespace scheds {
/**
 * @class PowerAwareTimer
 * @brief A parallel scheduler that considers power consumption.
 */
class PowerAwareTimer : public Parallel {
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
        explicit PowerAwareTimer(const std::weak_ptr<Engine>& sim);

        /**
         * @brief Updates the platform state based on power awareness.
         * @override
         */
        void update_platform() override;
};
} // namespace scheds

#endif