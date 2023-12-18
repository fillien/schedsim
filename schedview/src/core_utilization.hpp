#ifndef STATS_HPP
#define STATS_HPP

#include "traces.hpp"
#include <map>

namespace outputs::stats {
void print_utilizations(const std::multimap<double, traces::trace>& input);
} // namespace outputs::stats

#endif
