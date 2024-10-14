#ifndef ENERGY_HPP
#define ENERGY_HPP

#include "protocols/hardware.hpp"
#include <protocols/traces.hpp>

namespace outputs::energy {
auto compute_energy_consumption(
    const std::vector<std::pair<double, protocols::traces::trace>>& input,
    const protocols::hardware::hardware& hw) -> double;
} // namespace outputs::energy
#endif
