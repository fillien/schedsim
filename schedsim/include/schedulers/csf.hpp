#ifndef SCHED_M_MIN_HPP
#define SCHED_M_MIN_HPP

#include "../engine.hpp"
#include "../entity.hpp"
#include "parallel.hpp"
#include <memory>

class csf : public sched_parallel {
      private:
        std::size_t nb_active_procs{1};

      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        explicit csf(const std::weak_ptr<engine> sim) : sched_parallel(sim) {};

        void update_platform() override;
};

#endif
