#ifndef SCHED_M_MIN_HPP
#define SCHED_M_MIN_HPP

#include "../engine.hpp"
#include "parallel.hpp"
#include <memory>

class pa_m_min : public sched_parallel {
      private:
        std::size_t nb_active_procs{1};

      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        explicit pa_m_min(const std::weak_ptr<engine> sim) : sched_parallel(sim){};

        void on_active_utilization_updated() override;
};

#endif
