#ifndef STATS_HPP
#define STATS_HPP

#include "traces.hpp"
#include <map>

namespace outputs::stats {
void print_utilizations(const std::multimap<double, traces::trace>& input);
void print_nb_preemption(const std::multimap<double, traces::trace>& input);
void print_average_waiting_time(const std::multimap<double, traces::trace>& input);
} // namespace outputs::stats

#endif
