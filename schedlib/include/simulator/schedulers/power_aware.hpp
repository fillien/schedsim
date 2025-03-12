#ifndef SCHED_POWER_AWARE_HPP
#define SCHED_POWER_AWARE_HPP

#include <memory>
#include <simulator/entity.hpp>
#include <simulator/schedulers/parallel.hpp>

namespace scheds {
class PowerAware : public Parallel {
      protected:
        auto nb_active_procs() const -> std::size_t;

      public:
        /**
         * @brief Constructs a parallel scheduler with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit PowerAware(const std::weak_ptr<Engine>& sim) : Parallel(sim) {};

        void update_platform() override;
};
} // namespace scheds
#endif
