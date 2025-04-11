#ifndef SCHED_CSF_TIMER_HPP
#define SCHED_CSF_TIMER_HPP

#include <simulator/schedulers/dpm_dvfs.hpp>

namespace scheds {

/**
 * @brief A class that implements a CSF timer for dynamic power management (DPM) and
 *        dynamic voltage and frequency scaling (DVFS).
 */
class CsfTimer : public DpmDvfs {
      public:
        /**
         * @brief Constructor for the CsfTimer class.
         *
         * @param sim A weak pointer to the simulation engine.
         */
        explicit CsfTimer(const std::weak_ptr<Engine>& sim);

        /**
         * @brief Updates the platform state based on the CSF timer's logic.
         *        This function overrides the base class method.
         */
        void update_platform() override;

      private:
        /**
         * @brief Manages the DPM cooldown timers and transitions processors to inactive states.
         *
         * @param next_active_procs A lambda expression that determines which processors should be
         * active.
         */
        void manage_dpm_timer(auto next_active_procs);

        /**
         * @brief Manages the DVFS cooldown timer and changes the chip's frequency after a cooldown
         * period.
         *
         * @param next_freq A lambda expression that determines the target frequency.
         */
        void manage_dvfs_timer(auto next_freq);

        /**
         * @brief Timer used for DVFS cooldown.  When triggered, it changes the chip's frequency
         *        to freq_after_cooldown if it is different from the current frequency and removes
         * tasks from CPUs.
         */
        std::shared_ptr<Timer> timer_dvfs_cooldown = std::make_shared<Timer>(sim(), [this]() {
                if (chip()->freq() != chip()->ceil_to_mode(freq_after_cooldown)) {
                        for (const auto& proc : chip()->processors()) {
                                remove_task_from_cpu(proc);
                        }
                        chip()->dvfs_change_freq(freq_after_cooldown);
                }
        });

        /**
         * @brief A vector of timers used for DPM cooldown. Each timer corresponds to a processor's
         *        cooldown period before it can transition to an inactive state.
         */
        std::vector<std::shared_ptr<Timer>> timers_dpm_cooldown;

        /**
         * @brief The target frequency after the DVFS cooldown period has elapsed.
         */
        double freq_after_cooldown{0};
};
} // namespace scheds

#endif