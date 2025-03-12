#ifndef STATS_HPP
#define STATS_HPP

#include "protocols/traces.hpp"

#include <cstddef>
#include <vector>

namespace outputs::stats {
using logs_type = std::vector<std::pair<double, protocols::traces::trace>>;
auto count_nb_preemption(const logs_type& input) -> std::size_t;
auto count_nb_contextswitch(const logs_type& input) -> std::size_t;
auto count_average_waiting_time(const logs_type& input) -> double;
auto count_duration(const logs_type& input) -> double;
auto count_rejected(const logs_type& input) -> std::size_t;
auto count_cluster_migration(const logs_type& input) -> std::size_t;
auto count_arrivals(const logs_type& input) -> std::size_t;
auto count_core_state_request(const logs_type& input) -> std::size_t;
auto count_frequency_request(const logs_type& input) -> std::size_t;
} // namespace outputs::stats

#endif
