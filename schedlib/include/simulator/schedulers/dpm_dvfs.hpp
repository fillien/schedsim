#ifndef SCHED_DPMDVFS
#define SCHED_DPMDVFS

#include <simulator/schedulers/parallel.hpp>

namespace scheds {

/**
 * @brief Dynamic Power Management and Dynamic Voltage Frequency Scaling scheduler.
 */
class DpmDvfs : public Parallel {
      public:
        explicit DpmDvfs(Engine& sim) : Parallel(sim) {}

        auto cluster(Cluster* clu) -> void { Parallel::cluster(clu); }

      protected:
        auto compute_freq_min(
            const double& freq_max,
            const double& total_util,
            const double& max_util,
            const double& nb_procs) -> double;

        void adjust_active_processors(const std::size_t target_processors);

        void activate_next_core();

        auto nb_active_procs() const -> std::size_t;

        static constexpr double DVFS_COOLDOWN{Cluster::DVFS_DELAY * 2};
        static constexpr double DPM_COOLDOWN{Processor::DPM_DELAY * 2};

      private:
        void change_state_proc(const Processor::State& next_state, Processor* proc);

        void put_next_core_to_bed();
};

} // namespace scheds

#endif
