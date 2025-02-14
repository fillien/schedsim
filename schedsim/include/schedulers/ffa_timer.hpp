#ifndef SCHED_FFA_TIMER_HPP
#define SCHED_FFA_TIMER_HPP

#include <cstddef>
#include <engine.hpp>
#include <entity.hpp>
#include <memory>
#include <platform.hpp>
#include <processor.hpp>
#include <schedulers/parallel.hpp>
#include <timer.hpp>

namespace scheds {
class FfaTimer : public Parallel {
      private:
        std::size_t nb_active_procs{1};

        static constexpr double DVFS_COOLDOWN{Cluster::DVFS_DELAY * 2};
        static constexpr double DPM_COOLDOWN{Processor::DPM_DELAY * 2};

        std::shared_ptr<Timer> timer_dvfs_cooldown;
        std::vector<std::shared_ptr<Timer>> timers_dpm_cooldown;

        double freq_after_cooldown;

        auto compute_freq_min(
            const double& freq_max,
            const double& total_util,
            const double& max_util,
            const double& nb_procs) -> double;
        auto cores_on_timer() -> std::size_t;
        auto cores_on_sleep() -> std::size_t;
        void cancel_next_timer();
        void activate_next_core();
        void put_next_core_to_bed();
        void change_state_proc(
            const Processor::State& next_state, const std::shared_ptr<Processor>& proc);
        void adjust_active_processors(std::size_t target_processors);

      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        explicit FfaTimer(const std::weak_ptr<Engine>& sim);
        void update_platform() override;
};
} // namespace scheds

#endif
