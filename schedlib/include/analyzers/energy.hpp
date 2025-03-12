#ifndef ENERGY_HPP
#define ENERGY_HPP

#include <any>
#include <cstddef>
#include <protocols/hardware.hpp>
#include <protocols/traces.hpp>
#include <vector>

namespace outputs::energy {
auto compute_energy_consumption(
    const std::vector<std::pair<double, protocols::traces::trace>>& input,
    const protocols::hardware::Hardware& hw) -> std::map<std::string, std::vector<std::any>>;
} // namespace outputs::energy
#endif
