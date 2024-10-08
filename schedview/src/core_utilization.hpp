#ifndef STATS_HPP
#define STATS_HPP

#include <map>
#include <protocols/traces.hpp>

namespace outputs::stats {
void print_utilizations(const std::multimap<double, protocols::traces::trace>& input);
} // namespace outputs::stats

#endif
