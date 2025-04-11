#ifndef STATS_HPP
#define STATS_HPP

#include "protocols/hardware.hpp"
#include "protocols/traces.hpp"

#include <any>
#include <cstddef>
#include <map>
#include <vector>

namespace outputs::stats {
using logs_type = std::vector<std::pair<double, protocols::traces::trace>>;

/**
 * @brief Counts the number of preemption events in a log.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @return The number of preemption events.
 */
auto count_nb_preemption(const logs_type& input) -> std::size_t;

/**
 * @brief Counts the number of context switch events in a log.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @return The number of context switch events.
 */
auto count_nb_contextswitch(const logs_type& input) -> std::size_t;

/**
 * @brief Calculates the average waiting time from a log.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @return The average waiting time.
 */
auto count_average_waiting_time(const logs_type& input) -> double;

/**
 * @brief Calculates the total duration represented by a log.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @return The total duration.
 */
auto count_duration(const logs_type& input) -> double;

/**
 * @brief Counts the number of rejected events in a log.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @return The number of rejected events.
 */
auto count_rejected(const logs_type& input) -> std::size_t;

/**
 * @brief Counts the number of cluster migration events in a log.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @return The number of cluster migration events.
 */
auto count_cluster_migration(const logs_type& input) -> std::size_t;

/**
 * @brief Counts the number of arrival events in a log.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @return The number of arrival events.
 */
auto count_arrivals(const logs_type& input) -> std::size_t;

/**
 * @brief Counts the number of possible transition events in a log.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @return The number of possible transition events.
 */
auto count_possible_transition(const logs_type& input) -> std::size_t;

/**
 * @brief Counts the number of core state request events in a log.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @return The number of core state request events.
 */
auto count_core_state_request(const logs_type& input) -> std::size_t;

/**
 * @brief Counts the number of frequency request events in a log.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @return The number of frequency request events.
 */
auto count_frequency_request(const logs_type& input) -> std::size_t;

/**
 * @brief Counts the cores utilization from a log and hardware description.
 *
 * @param input The input log as a vector of pairs (timestamp, trace).
 * @param hw The hardware description.
 * @return A map containing core names and their utilization vectors.
 */
auto count_cores_utilization(const logs_type& input, const protocols::hardware::Hardware& hw)
    -> std::map<std::string, std::vector<std::any>>;

} // namespace outputs::stats

#endif