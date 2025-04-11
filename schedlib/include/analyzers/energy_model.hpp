#ifndef ENERGY_MODEL
#define ENERGY_MODEL

#include <protocols/hardware.hpp>

namespace energy {

/**
 * @brief Computes the power consumption of a hardware cluster given its frequency.
 *
 * This function calculates the power consumed by a `protocols::hardware::Cluster`
 * based on its operating frequency and internal characteristics defined within the
 * `hw` object.  The calculation is specific to the energy model implemented.
 *
 * @param freq The operating frequency of the hardware cluster in Hz.
 * @param hw   A constant reference to a `protocols::hardware::Cluster` object representing the
 * hardware configuration.
 *
 * @return The power consumption in Watts (W).
 */
auto compute_power(const double& freq, const protocols::hardware::Cluster& hw) -> double;

} // namespace energy

#endif