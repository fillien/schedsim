#ifndef STATS_HPP
#define STATS_HPP

#include <protocols/traces.hpp>
#include <vector>

namespace outputs::stats {
void print_utilizations(const std::vector<std::pair<double, protocols::traces::trace>>& input);
void print_nb_preemption(const std::vector<std::pair<double, protocols::traces::trace>>& input);
void print_nb_contextswitch(const std::vector<std::pair<double, protocols::traces::trace>>& input);
void print_average_waiting_time(
    const std::vector<std::pair<double, protocols::traces::trace>>& input);
void print_duration(const std::vector<std::pair<double, protocols::traces::trace>>& input);
void print_rejected(const std::vector<std::pair<double, protocols::traces::trace>>& input);
void print_core_state_request_count(
    const std::vector<std::pair<double, protocols::traces::trace>>& input);
void print_frequency_request_count(
    const std::vector<std::pair<double, protocols::traces::trace>>& input);
} // namespace outputs::stats

#endif
