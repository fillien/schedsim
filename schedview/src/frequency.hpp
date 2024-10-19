#ifndef FREQUENCY_HPP
#define FREQUENCY_HPP

#include <any>
#include <protocols/traces.hpp>
#include <vector>

namespace outputs::frequency {
auto track_frequency_changes(const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>;
auto track_cores_changes(const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>;
} // namespace outputs::frequency
#endif
