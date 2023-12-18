#ifndef DEADLINE_MISSES
#define DEADLINE_MISSES

#include "traces.hpp"
#include <cstddef>
#include <map>

namespace outputs::stats {

auto detect_deadline_misses(const std::multimap<double, traces::trace>& logs)
    -> std::map<std::size_t, std::pair<std::size_t, std::size_t>>;

void print_task_deadline_missed_count(
    const std::map<std::size_t, std::pair<std::size_t, std::size_t>>& deadline_stats,
    std::size_t tid);

void print_task_deadline_missed_rate(
    const std::map<std::size_t, std::pair<std::size_t, std::size_t>>& deadline_stats,
    std::size_t tid);

void print_deadline_missed_count(
    const std::map<std::size_t, std::pair<std::size_t, std::size_t>>& deadline_stats);

void print_deadline_missed_rate(
    const std::map<std::size_t, std::pair<std::size_t, std::size_t>>& deadline_stats);
} // namespace outputs::stats
#endif
