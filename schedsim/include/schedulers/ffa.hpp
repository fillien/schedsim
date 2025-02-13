#ifndef SCHED_FFA_HPP
#define SCHED_FFA_HPP

#include <cstddef>
#include <engine.hpp>
#include <entity.hpp>
#include <memory>
#include <processor.hpp>
#include <schedulers/parallel.hpp>
#include <timer.hpp>

namespace scheds {
class ffa : public parallel {
      private:
        std::size_t nb_active_procs{1};

        static auto compute_freq_min(
            const double& freq_max,
            const double& total_util,
            const double& max_util,
            const double& nb_procs) -> double;
        auto cores_on_timer() -> std::size_t;
        void cancel_next_timer();
        void activate_next_core();
        void put_next_core_to_bed();
        void change_state_proc(
            const processor::state& next_state, const std::shared_ptr<processor>& proc);
        void adjust_active_processors(std::size_t target_processors);

      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        explicit ffa(const std::weak_ptr<engine>& sim);
        void set_cluster(const std::weak_ptr<Cluster>& clu);
        void update_platform() override;
};
} // namespace scheds

#endif
