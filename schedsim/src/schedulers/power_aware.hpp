#ifndef SCHED_POWER_AWARE_HPP
#define SCHED_POWER_AWARE_HPP

#include "../entity.hpp"
#include "../processor.hpp"
#include "../scheduler.hpp"
#include "parallel.hpp"

#include <iostream>
#include <memory>

class sched_power_aware : public sched_parallel {
      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        /**
         * @brief Constructs a parallel scheduler with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit sched_power_aware(const std::weak_ptr<engine> sim) : sched_parallel(sim){};

        void on_active_utilization_updated() override;
};

#endif
