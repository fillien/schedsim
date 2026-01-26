#ifndef SCHED_CSF_TIMER_HPP
#define SCHED_CSF_TIMER_HPP

#include <simulator/schedulers/dpm_dvfs.hpp>

#include <memory>
#include <vector>

namespace scheds {

class CsfTimer : public DpmDvfs {
      public:
        explicit CsfTimer(Engine& sim);

        void update_platform() override;

      private:
        void manage_dpm_timer(auto next_active_procs);
        void manage_dvfs_timer(auto next_freq);

        std::unique_ptr<Timer> timer_dvfs_cooldown;
        std::vector<std::unique_ptr<Timer>> timers_dpm_cooldown;
        double freq_after_cooldown{0};
};

} // namespace scheds

#endif
