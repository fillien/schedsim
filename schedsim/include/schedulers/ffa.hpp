#ifndef SCHED_F_MIN_HPP
#define SCHED_F_MIN_HPP

#include "../engine.hpp"
#include "../entity.hpp"
#include "parallel.hpp"
#include <memory>

class ffa : public sched_parallel {
      private:
        std::size_t nb_active_procs{1};
        auto compute_freq_min(
            const double& freq_max,
            const double& total_util,
            const double& max_util,
            const double& nb_procs) -> double;

      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        explicit ffa(const std::weak_ptr<engine> sim) : sched_parallel(sim) {};

        void update_platform() override;
};

#endif
