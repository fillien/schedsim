#ifndef SCHED_CSF_HPP
#define SCHED_CSF_HPP

#include <simulator/schedulers/dpm_dvfs.hpp>

namespace scheds {

/**
 * @brief Represents a Clock Speed Frequency (CSF) scheduler.
 *
 * This class inherits from DpmDvfs and implements a specific scheduling policy
 * based on clock speed frequency.  It's designed to manage dynamic voltage and
 * frequency scaling (DVFS) within the simulation environment.
 */
class Csf : public DpmDvfs {
      public:
        /**
         * @brief Constructor for the Csf class.
         *
         * Initializes a Csf scheduler instance, linking it to the provided simulation engine.
         *
         * @param sim A weak pointer to the Engine object representing the simulator.  Using
         *            a weak pointer avoids circular dependencies and allows the Engine to be
         *            destroyed independently if this scheduler is no longer needed.
         */
        explicit Csf(const std::weak_ptr<Engine>& sim) : DpmDvfs(sim) {}

        /**
         * @brief Updates the platform state based on the CSF scheduling policy.
         *
         * This method overrides the virtual function from the base class (DpmDvfs).
         * It contains the core logic for determining and applying DVFS settings to the simulated
         * platform.
         */
        void update_platform() override;
};

} // namespace scheds

#endif