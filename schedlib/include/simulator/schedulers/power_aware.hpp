#ifndef SCHED_POWER_AWARE_HPP
#define SCHED_POWER_AWARE_HPP

#include <simulator/schedulers/parallel.hpp>

namespace scheds {

class PowerAware : public Parallel {
      protected:
        auto nb_active_procs() const -> std::size_t;

      public:
        explicit PowerAware(Engine& sim) : Parallel(sim) {};

        void update_platform() override;
};

} // namespace scheds

#endif
