#include "deadline_misses.hpp"
#include <protocols/traces.hpp>

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace {
struct job_finished {
        std::size_t tid;
};
struct job_deadline {
        std::size_t tid;
};

using job_events = std::variant<job_finished, job_deadline>;

/**
 * @brief Checks if a job with the specified timestamp and task ID has passed admission test.
 *
 * This function searches for a job with the given timestamp and task ID in the provided logs.
 * If a job with the specified timestamp and task ID is found and it is a rejected task, the
 * function returns false; otherwise, it returns true.
 *
 * @param logs A multimap containing timestamped traces of events.
 * @param timestamp The timestamp to search for in the logs.
 * @param tid The task ID to check for acceptance.
 * @return true if the job is accepted, false otherwise.
 */
auto is_accepted_job(
    const std::multimap<double, protocols::traces::trace>& logs,
    const double& timestamp,
    std::size_t tid) -> bool
{
        namespace traces = protocols::traces;
        auto range = logs.equal_range(timestamp);
        return std::find_if(
                   range.first, range.second, [&tid](std::pair<double, traces::trace> tra) {
                           if (auto* const evt = std::get_if<traces::task_rejected>(&tra.second)) {
                                   return tid == evt->task_id;
                           }
                           return false;
                   }) == range.second;
}

/**
 * @brief Filters trace logs to retain non-rejected job arrival and job finished events.
 *
 * This function takes unfiltered logs and filters them to include only non-rejected job arrival
 * events and job finished events. For each job arrival event that passes the admission test,
 * it inserts an absolute deadline event into the filtered logs. Job finished events are
 * transferred directly to the output.
 *
 * @param unfiltered_logs The unfiltered trace logs containing various events.
 * @return A multimap containing filtered job events with timestamps.
 */
auto filter_logs(std::multimap<double, protocols::traces::trace> unfiltered_logs)
    -> std::multimap<double, job_events>
{
        namespace traces = protocols::traces;
        std::multimap<double, job_events> filtered_input;

        // Iterate through unfiltered logs and apply filtering criteria
        for (auto itr = std::begin(unfiltered_logs); itr != std::end(unfiltered_logs); ++itr) {
                const auto& timestamp{(*itr).first};
                if (auto* const evt = std::get_if<traces::job_arrival>(&(itr->second))) {
                        // Detect if the task has passed the admission test
                        if (is_accepted_job(unfiltered_logs, timestamp, evt->task_id)) {
                                // Insert the absolute deadline event
                                filtered_input.insert(
                                    {evt->deadline, job_deadline{.tid = evt->task_id}});
                        }
                }
                else if (auto* const evt = std::get_if<traces::job_finished>(&(itr->second))) {
                        // Just transfert job_finished event to the output
                        filtered_input.insert({timestamp, job_finished{.tid = evt->task_id}});
                }
        }

        return filtered_input;
}

/**
 * @brief Removes the next event of a specified type for a given task ID after a certain timestamp.
 *
 * This function searches for the next event of the specified type (template parameter T) associated
 * with the given task ID that occurs after the specified timestamp. If such an event is found, it
 * is removed from the provided logs.
 *
 * @tparam T The type of the event to be removed.
 * @param logs The multimap containing timestamped job events.
 * @param timestamp The timestamp after which to search for the next event.
 * @param tid The task ID for which to remove the next event.
 */
template <typename T>
void remove_next_event(
    std::multimap<double, job_events>& logs, const double& timestamp, std::size_t tid)
{
        // Search for the next deadline event for the specified task ID after the given timestamp
        auto result = std::find_if(
            std::begin(logs), std::end(logs), [&](const std::pair<double, job_events>& future_evt) {
                    if (auto next_evt = std::get_if<T>(&future_evt.second)) {
                            return future_evt.first > timestamp && next_evt->tid == tid;
                    }
                    return false;
            });
        // If the next event is found, remove it from the logs
        if (result != std::end(logs)) { logs.erase(result); }
}

/**
 * @brief Updates deadline statistics for a specified task ID.
 *
 * This function increments the total number of deadlines and, if a deadline is missed, increments
 * the corresponding missed deadline count for the specified task ID in the provided statistics map.
 * If the task ID is not present in the map, a new entry is created.
 *
 * @param tid The task ID for which to update the deadline statistics.
 * @param stats The map containing task ID as keys and a pair of total and missed deadline counts as
 * values.
 * @param deadline_missed A boolean indicating whether the deadline for the task was missed.
 */
void increase_deadline_stats(
    std::size_t tid,
    std::map<std::size_t, std::pair<std::size_t, std::size_t>>& stats,
    bool deadline_missed)
{
        std::pair<std::size_t, std::size_t> updated_value{0, 0};
        if (stats.contains(tid)) { updated_value = stats.at(tid); }
        updated_value.first++;
        if (deadline_missed) { updated_value.second++; }
        stats.insert_or_assign(tid, updated_value);
}
} // namespace

namespace outputs::stats {

/**
 * @brief Detects deadline misses for tasks based on trace logs.
 *
 * @param logs The multimap containing timestamped traces of events.
 * @return A map of task IDs with statistics: the number of total jobs and the number of missed
 * deadlines.
 */
auto detect_deadline_misses(const std::multimap<double, protocols::traces::trace>& logs)
    -> std::map<std::size_t, std::pair<std::size_t, std::size_t>>
{
        // Filter input traces to only save job arrivals that are not rejected, and job finished
        auto filtered_logs = filter_logs(logs);

        // Map of tasks with first the number of jobs and second the number of deadline missed
        std::map<std::size_t, std::pair<std::size_t, std::size_t>> tasks_deadline_rate;

        // Build stats
        for (const auto& [timestamp, event] : filtered_logs) {
                if (const auto* evt = std::get_if<job_finished>(&event)) {
                        remove_next_event<job_deadline>(filtered_logs, timestamp, evt->tid);
                        increase_deadline_stats(evt->tid, tasks_deadline_rate, false);
                }
                else if (const auto* evt = std::get_if<job_deadline>(&event)) {
                        remove_next_event<job_finished>(filtered_logs, timestamp, evt->tid);
                        increase_deadline_stats(evt->tid, tasks_deadline_rate, true);
                }
        }

        return tasks_deadline_rate;
}

/**
 * @brief Prints the count of missed deadlines for a specific task.
 *
 * This function prints the count of missed deadlines along with the total number of jobs
 * for a specified task. It throws an exception if the task ID is not present in the provided
 * deadline statistics map.
 *
 * @param deadline_stats The map containing task IDs with associated statistics.
 * @param tid The task ID for which to print the deadline statistics.
 * @throws std::out_of_range If the specified task ID is not found in the deadline statistics.
 */
void print_task_deadline_missed_count(
    const std::map<std::size_t, std::pair<std::size_t, std::size_t>>& deadline_stats,
    std::size_t tid)
{
        if (!deadline_stats.contains(tid)) { throw std::out_of_range("Unknown task"); }

        auto [jobs_count, deadline_missed_count] = deadline_stats.at(tid);
        std::cout << deadline_missed_count << "/" << jobs_count << " deadlines missed" << std::endl;
}

/**
 * @brief Prints the percentage of missed deadlines for a specific task.
 *
 * This function prints the percentage of missed deadlines relative to the total number of jobs
 * for a specified task. It throws an exception if the task ID is not present in the provided
 * deadline statistics map.
 *
 * @param deadline_stats The map containing task IDs with associated statistics.
 * @param tid The task ID for which to print the deadline miss rate.
 * @throws std::out_of_range If the specified task ID is not found in the deadline statistics.
 */
void print_task_deadline_missed_rate(
    const std::map<std::size_t, std::pair<std::size_t, std::size_t>>& deadline_stats,
    std::size_t tid)
{
        if (!deadline_stats.contains(tid)) { throw std::out_of_range("Unknown task"); }

        auto [jobs_count, deadline_missed_count] = deadline_stats.at(tid);
        std::cout << std::setprecision(4)
                  << (static_cast<double>(deadline_missed_count) /
                      static_cast<double>(jobs_count)) *
                         100
                  << "% deadlines missed" << std::endl;
}

/**
 * @brief Prints the total count of missed deadlines across all tasks.
 *
 * This function calculates and prints the total count of missed deadlines and the total number
 * of jobs across all tasks based on the provided deadline statistics map.
 *
 * @param deadline_stats The map containing task IDs with associated statistics.
 */
void print_deadline_missed_count(
    const std::map<std::size_t, std::pair<std::size_t, std::size_t>>& deadline_stats)
{
        std::size_t sum_of_jobs{0};
        std::size_t sum_of_deadline_missed{0};

        for (const auto& [_, task] : deadline_stats) {
                const auto& jobs_count{task.first};
                const auto& deadline_missed_count{task.second};
                sum_of_jobs += jobs_count;
                sum_of_deadline_missed += deadline_missed_count;
        }

        std::cout << sum_of_deadline_missed << "/" << sum_of_jobs << " deadlines missed"
                  << std::endl;
}

/**
 * @brief Prints the overall percentage of missed deadlines across all tasks.
 *
 * This function calculates and prints the overall percentage of missed deadlines relative to
 * the total number of jobs across all tasks based on the provided deadline statistics map.
 *
 * @param deadline_stats The map containing task IDs with associated statistics.
 */
void print_deadline_missed_rate(
    const std::map<std::size_t, std::pair<std::size_t, std::size_t>>& deadline_stats)
{
        std::size_t sum_of_jobs{0};
        std::size_t sum_of_deadline_missed{0};

        for (const auto& [_, task] : deadline_stats) {
                const auto& jobs_count{task.first};
                const auto& deadline_missed_count{task.second};
                sum_of_jobs += jobs_count;
                sum_of_deadline_missed += deadline_missed_count;
        }

        std::cout << std::setprecision(4)
                  << (static_cast<double>(sum_of_deadline_missed) /
                      static_cast<double>(sum_of_jobs)) *
                         100
                  << std::endl;
}

} // namespace outputs::stats
