#ifndef STATS_HPP
#define STATS_HPP

#include <protocols/traces.hpp>
#include <vector>

namespace outputs::stats {
void print_utilizations(const std::vector<std::pair<double, protocols::traces::trace>>& input);
} // namespace outputs::stats

#endif
