#ifndef SCHED_POWER_AWARE_TIMER_HPP
#define SCHED_POWER_AWARE_TIMER_HPP

#include <simulator/schedulers/parallel.hpp>

namespace scheds {

class PowerAwareTimer : public Parallel {
      protected:
        auto nb_active_procs() const -> std::size_t;

      public:
        explicit PowerAwareTimer(Engine& sim);

        void update_platform() override;
};

} // namespace scheds

#endif
