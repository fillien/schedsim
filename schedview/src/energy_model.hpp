#ifndef ENERGY_MODEL
#define ENERGY_MODEL

#include "protocols/hardware.hpp"

namespace energy {
auto compute_power(const double& freq, const protocols::hardware::cluster& hw) -> double;
} // namespace energy

#endif
