#ifndef SCHED_POWER_AWARE_TIMER_HPP
#define SCHED_POWER_AWARE_TIMER_HPP

#include <entity.hpp>
#include <memory>
#include <schedulers/parallel.hpp>

namespace scheds {
class PowerAwareTimer : public Parallel {
      protected:
        auto nb_active_procs() const -> std::size_t;

      public:
        /**
         * @brief Constructs a parallel scheduler with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit PowerAwareTimer(const std::weak_ptr<Engine>& sim);

        void update_platform() override;
};
} // namespace scheds

#endif
