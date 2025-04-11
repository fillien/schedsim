#ifndef DEADLINE_MISSES
#define DEADLINE_MISSES

#include <cstddef>
#include <map>
#include <protocols/traces.hpp>
#include <utility>
#include <vector>

namespace outputs::stats {

/**
 * @brief Type alias for a vector of log entries, where each entry consists of a timestamp (double)
 * and a trace object.
 */
using logs_type = std::vector<std::pair<double, protocols::traces::trace>>;

/**
 * @brief Type alias for a map storing deadline statistics.
 *
 * The keys are task IDs (size_t).  The values are pairs: the first element is the number of times
 * the task was attempted, and the second is the number of times it missed its deadline.
 */
using deadline_type = std::map<std::size_t, std::pair<std::size_t, std::size_t>>;

/**
 * @brief Detects deadline misses from a log of events.
 *
 * This function analyzes the provided logs and identifies instances where tasks miss their
 * deadlines.  It returns a map containing statistics about these misses for each task.
 *
 * @param logs The input log data, consisting of timestamps and trace objects.
 * @return A map representing deadline statistics, keyed by task ID.
 */
auto detect_deadline_misses(const logs_type& logs) -> deadline_type;

/**
 * @brief Counts the number of times a specific task missed its deadline.
 *
 * This function retrieves the deadline miss count for a given task from the provided deadline
 * statistics map.
 *
 * @param deadline_stats The map containing deadline statistics for all tasks.
 * @param tid The ID of the task to query.
 * @return The number of times the specified task missed its deadline.
 */
auto count_task_deadline_missed(const deadline_type& deadline_stats, const std::size_t tid)
    -> std::size_t;

/**
 * @brief Counts the total number of deadline misses across all tasks.
 *
 * This function calculates the sum of deadline misses for all tasks stored in the provided deadline
 * statistics map.
 *
 * @param deadline_stats The map containing deadline statistics for all tasks.
 * @return The total number of deadline misses.
 */
auto count_deadline_missed(const deadline_type& deadline_stats) -> std::size_t;

} // namespace outputs::stats
#endif