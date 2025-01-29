#ifndef ENERGY_HPP
#define ENERGY_HPP

#include <cstddef>
#include <protocols/hardware.hpp>
#include <protocols/traces.hpp>

namespace outputs::energy {
auto cpu_to_cluster(const protocols::hardware::hardware& hw, std::size_t cpu) -> std::size_t;
auto compute_energy_consumption(
    const std::vector<std::pair<double, protocols::traces::trace>>& input,
    const protocols::hardware::hardware& hw) -> double;
} // namespace outputs::energy
#endif
