#ifndef FREQUENCY_HPP
#define FREQUENCY_HPP

#include "protocols/traces.hpp"

#include <any>
#include <map>
#include <string>
#include <vector>

namespace outputs::frequency {

/**
 * @brief Tracks frequency changes from a vector of trace data.
 *
 * This function analyzes the input trace data and returns a map where keys are
 * identifiers (e.g., component names) and values are vectors containing the
 * observed frequency changes over time. The type of the elements in the vector is `std::any` to
 * allow for flexibility in representing different types of frequency data.
 *
 * @param input A vector of pairs, where each pair contains a timestamp (double) and a trace object
 * (protocols::traces::trace).
 * @return A map containing frequency changes keyed by identifier.  Returns an empty map if the
 * input is empty or invalid.
 */
auto track_frequency_changes(const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>;

/**
 * @brief Tracks core changes from a vector of trace data.
 *
 * This function analyzes the input trace data and returns a map where keys are
 * identifiers (e.g., core names) and values are vectors containing the
 * observed core changes over time. The type of the elements in the vector is `std::any` to allow
 * for flexibility in representing different types of core data.
 *
 * @param input A vector of pairs, where each pair contains a timestamp (double) and a trace object
 * (protocols::traces::trace).
 * @return A map containing core changes keyed by identifier. Returns an empty map if the input is
 * empty or invalid.
 */
auto track_cores_changes(const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>;

/**
 * @brief Tracks configuration changes from a vector of trace data.
 *
 * This function analyzes the input trace data and returns a map where keys are
 * identifiers (e.g., configuration parameter names) and values are vectors containing the
 * observed configuration changes over time. The type of the elements in the vector is `std::any` to
 * allow for flexibility in representing different types of configuration data.
 *
 * @param input A vector of pairs, where each pair contains a timestamp (double) and a trace object
 * (protocols::traces::trace).
 * @return A map containing configuration changes keyed by identifier. Returns an empty map if the
 * input is empty or invalid.
 */
auto track_config_changes(const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>;

} // namespace outputs::frequency
#endif