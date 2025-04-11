#ifndef ENERGY_HPP
#define ENERGY_HPP

#include <any>
#include <cstddef>
#include <map>
#include <protocols/hardware.hpp>
#include <protocols/traces.hpp>
#include <string>
#include <vector>

namespace outputs::energy {

/**
 * @brief Computes the energy consumption based on input traces and hardware specifications.
 *
 * This function takes a vector of trace data points (time, value) and hardware information
 * as input. It calculates the energy consumed by different components based on these inputs
 * and returns a map containing vectors of energy values for each component.  The returned map's
 * keys are strings representing the component names. The values are vectors of `std::any` to allow
 * flexibility in storing different data types (e.g., double, int) as energy consumption values.
 *
 * @param input A vector of pairs where the first element is a time value (double) and the second
 *              element is a trace value (protocols::traces::trace).  Represents the system's
 * activity over time.
 * @param hw    A reference to a `protocols::hardware::Hardware` object representing the hardware
 * configuration.
 *
 * @return A map where keys are component names (strings) and values are vectors of energy
 * consumption values for that component (std::vector<std::any>).  Returns an empty map if there's
 * an error or no data to process.
 */
auto compute_energy_consumption(
    const std::vector<std::pair<double, protocols::traces::trace>>& input,
    const protocols::hardware::Hardware& hw) -> std::map<std::string, std::vector<std::any>>;

} // namespace outputs::energy
#endif