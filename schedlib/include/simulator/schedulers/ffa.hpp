#ifndef SCHED_FFA_HPP
#define SCHED_FFA_HPP

#include <simulator/schedulers/dpm_dvfs.hpp>

namespace scheds {

/**
 * @brief Represents a First-Fit Allocation (FFA) scheduler, inheriting from DpmDvfs.
 *
 * This class implements the FFA scheduling algorithm for dynamic power management and DVFS.
 */
class Ffa : public DpmDvfs {
      public:
        /**
         * @brief Constructor for the Ffa class.
         *
         * @param sim A weak pointer to the simulation engine.
         */
        explicit Ffa(const std::weak_ptr<Engine>& sim) : DpmDvfs(sim) {}

        /**
         * @brief Updates the platform state based on the FFA scheduling algorithm.
         *
         * This function overrides the base class's update_platform method to implement the specific
         * logic for FFA.
         */
        void update_platform() override;
};

} // namespace scheds

#endif