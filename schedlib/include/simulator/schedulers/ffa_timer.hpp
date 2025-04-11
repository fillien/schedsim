#ifndef SCHED_FFA_TIMER_HPP
#define SCHED_FFA_TIMER_HPP

#include <simulator/schedulers/dpm_dvfs.hpp>

namespace scheds {
/**
 * @brief A scheduler that manages Dynamic Power Management (DPM) and Dynamic Voltage and Frequency
 * Scaling (DVFS) using a fixed frequency after cooldown period.
 */
class FfaTimer : public DpmDvfs {
      public:
        /**
         * @brief Constructor for the FfaTimer class.
         *
         * @param sim A weak pointer to the simulation engine.
         */
        explicit FfaTimer(const std::weak_ptr<Engine>& sim);

        /**
         * @brief Updates the platform state based on the scheduler's logic.
         *        This function overrides the base class method.
         */
        void update_platform() override;

      private:
        /**
         * @brief Manages the DPM timer, handling processor activation and deactivation.
         *
         * @param next_active_procs A lambda expression that returns the next set of active
         * processors.
         */
        void manage_dpm_timer(auto next_active_procs);

        /**
         * @brief Manages the DVFS timer, handling frequency scaling.
         *
         * @param next_freq A lambda expression that returns the next desired frequency.
         */
        void manage_dvfs_timer(auto next_freq);

        /**
         * @brief Timer used to control the cooldown period for DVFS changes.
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
         * @brief A vector of timers used to control the cooldown period for DPM changes.
         */
        std::vector<std::shared_ptr<Timer>> timers_dpm_cooldown;

        /**
         * @brief The frequency to set after the DVFS cooldown period.
         */
        double freq_after_cooldown{0};
};
} // namespace scheds

#endif