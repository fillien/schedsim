#ifndef SCHED_CSF_HPP
#define SCHED_CSF_HPP

#include <cstddef>
#include <engine.hpp>
#include <entity.hpp>
#include <memory>
#include <processor.hpp>
#include <schedulers/parallel.hpp>

namespace scheds {
class Csf : public Parallel {
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
            const Processor::State& next_state, const std::shared_ptr<Processor>& proc);

      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        explicit Csf(const std::weak_ptr<Engine>& sim);
        void set_cluster(const std::weak_ptr<Cluster>& clu);
        void update_platform() override;
};
} // namespace scheds

#endif
