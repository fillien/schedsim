#ifndef SCHED_CSF_HPP
#define SCHED_CSF_HPP

#include "../engine.hpp"
#include "../entity.hpp"
#include "parallel.hpp"
#include "processor.hpp"
#include <cstddef>
#include <memory>

namespace scheds {
class csf : public parallel {
      private:
        std::size_t nb_active_procs{1};

        auto compute_freq_min(
            const double& freq_max,
            const double& total_util,
            const double& max_util,
            const double& nb_procs) -> double;
        void activate_next_core();
        void put_next_core_to_bed();
        void adjust_active_processors(std::size_t target_processors);
        void change_state_proc(
            const processor::state& next_state, const std::shared_ptr<processor>& proc);

      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        explicit csf(const std::weak_ptr<engine>& sim);
        void set_cluster(const std::weak_ptr<cluster>& clu);
        void update_platform() override;
};
} // namespace scheds

#endif
