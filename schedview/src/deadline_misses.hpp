#ifndef DEADLINE_MISSES
#define DEADLINE_MISSES

#include <cstddef>
#include <map>
#include <protocols/traces.hpp>

namespace outputs::stats {
using logs_type = std::vector<std::pair<double, protocols::traces::trace>>;
using deadline_type = std::map<std::size_t, std::pair<std::size_t, std::size_t>>;

auto detect_deadline_misses(const logs_type& logs) -> deadline_type;

auto count_task_deadline_missed(const deadline_type& deadline_stats, const std::size_t tid)
    -> std::size_t;

auto count_deadline_missed(const deadline_type& deadline_stats) -> std::size_t;
} // namespace outputs::stats
#endif
