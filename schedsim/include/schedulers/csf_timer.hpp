#ifndef SCHED_CSF_TIMER_HPP
#define SCHED_CSF_TIMER_HPP

#include <schedulers/dpm_dvfs.hpp>

namespace scheds {

class CsfTimer : public DpmDvfs {
      public:
        explicit CsfTimer(const std::weak_ptr<Engine>& sim);
        void update_platform() override;

      private:
        std::shared_ptr<Timer> timer_dvfs_cooldown;
        std::vector<std::shared_ptr<Timer>> timers_dpm_cooldown;
        double freq_after_cooldown;
};
} // namespace scheds

#endif
