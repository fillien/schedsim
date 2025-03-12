#ifndef SCHED_CSF_TIMER_HPP
#define SCHED_CSF_TIMER_HPP

#include <schedulers/dpm_dvfs.hpp>

namespace scheds {

class CsfTimer : public DpmDvfs {
      public:
        explicit CsfTimer(const std::weak_ptr<Engine>& sim);
        void update_platform() override;

      private:
        void manage_dpm_timer(auto next_active_procs);
        void manage_dvfs_timer(auto next_freq);
        std::shared_ptr<Timer> timer_dvfs_cooldown = std::make_shared<Timer>(sim(), [this]() {
                if (chip()->freq() != chip()->ceil_to_mode(freq_after_cooldown)) {
                        for (const auto& proc : chip()->processors()) {
                                remove_task_from_cpu(proc);
                        }
                        chip()->dvfs_change_freq(freq_after_cooldown);
                }
        });
        std::vector<std::shared_ptr<Timer>> timers_dpm_cooldown;
        double freq_after_cooldown{0};
};
} // namespace scheds

#endif
