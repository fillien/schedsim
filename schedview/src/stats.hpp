#ifndef STATS_HPP
#define STATS_HPP

#include <map>
#include <protocols/traces.hpp>

namespace outputs::stats {
void print_utilizations(const std::multimap<double, protocols::traces::trace>& input);
void print_nb_preemption(const std::multimap<double, protocols::traces::trace>& input);
void print_nb_contextswitch(const std::multimap<double, protocols::traces::trace>& input);
void print_average_waiting_time(const std::multimap<double, protocols::traces::trace>& input);
void print_duration(const std::multimap<double, protocols::traces::trace>& input);
void print_rejected(const std::multimap<double, protocols::traces::trace>& input);
void print_core_state_request_count(const std::multimap<double, protocols::traces::trace>& input);
void print_frequency_request_count(const std::multimap<double, protocols::traces::trace>& input);
} // namespace outputs::stats

#endif
