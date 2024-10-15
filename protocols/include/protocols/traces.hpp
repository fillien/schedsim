#ifndef TRACES_HPP
#define TRACES_HPP

#include "nlohmann/json_fwd.hpp"
#include <filesystem>
#include <map>
#include <rapidjson/document.h>
#include <variant>

namespace protocols::traces {

/**
 * @brief Represents a rescheduling event.
 */
struct resched {};

/**
 * @brief Represents a simulation finished event.
 */
struct sim_finished {};

/**
 * @brief Represents a job arrival event.
 */
struct job_arrival {
        std::size_t task_id; /**< ID of the task. */
        double duration;     /**< Duration of the job. */
        double deadline;     /**< Absolut deadline of the job. */
};

/**
 * @brief Represents a job finished event.
 */
struct job_finished {
        std::size_t task_id; /**< ID of the task. */
};

/**
 * @brief Represents a processor activated event.
 */
struct proc_activated {
        std::size_t proc_id; /**< ID of the processor. */
};

/**
 * @brief Represents a processor idled event.
 */
struct proc_idled {
        std::size_t proc_id; /**< ID of the processor. */
};

/**
 * @brief Represents a processor activated event.
 */
struct proc_sleep {
        std::size_t proc_id; /**< ID of the processor. */
};

/**
 * @brief Represents a service budget replenished event.
 */
struct serv_budget_replenished {
        std::size_t task_id; /**< ID of the task. */
        double budget;       /**< Replenished budget. */
};

/**
 * @brief Represents a service inactive event.
 */
struct serv_inactive {
        std::size_t task_id; /**< ID of the task. */
        double utilization;  /**< Utilization of the task. */
};

/**
 * @brief Represents a service budget exhausted event.
 */
struct serv_budget_exhausted {
        std::size_t task_id; /**< ID of the task. */
};

/**
 * @brief Represents a non-continuous service event.
 */
struct serv_non_cont {
        std::size_t task_id; /**< ID of the task. */
};

/**
 * @brief Represents a service postpone event.
 */
struct serv_postpone {
        std::size_t task_id; /**< ID of the task. */
        double deadline;     /**< New deadline after postponement. */
};

/**
 * @brief Represents a service ready event.
 */
struct serv_ready {
        std::size_t task_id; /**< ID of the task. */
        double deadline;     /**< Deadline of the task. */
        double utilization;  /**< Utilization of the task. */
};

/**
 * @brief Represents a service running event.
 */
struct serv_running {
        std::size_t task_id; /**< ID of the task. */
};

/**
 * @brief Represents a task preempted event.
 */
struct task_preempted {
        std::size_t task_id; /**< ID of the task. */
};

/**
 * @brief Represents a task scheduled event.
 */
struct task_scheduled {
        std::size_t task_id; /**< ID of the task. */
        std::size_t proc_id; /**< ID of the processor. */
};

/**
 * @brief Represents a task rejected event.
 */
struct task_rejected {
        std::size_t task_id; /**< ID of the task. */
};

/**
 * @brief Represents a virtual time update event.
 */
struct virtual_time_update {
        std::size_t task_id; /**< ID of the task. */
        double virtual_time; /**< Updated virtual time. */
};

/**
 * @brief Represents a frequency scaling of the platform
 */
struct frequency_update {
        double frequency; /**< New frequency. */
};

/**
 * @brief Type representing various trace events.
 */
using trace = std::variant<
    resched,
    sim_finished,
    virtual_time_update,
    frequency_update,
    job_arrival,
    job_finished,
    proc_activated,
    proc_idled,
    proc_sleep,
    serv_budget_exhausted,
    serv_inactive,
    serv_budget_replenished,
    serv_non_cont,
    serv_postpone,
    serv_ready,
    serv_running,
    task_preempted,
    task_scheduled,
    task_rejected>;

/**
 * @brief Converts a trace event to JSON format.
 * @param log The trace event to convert.
 * @return JSON representation of the trace event.
 */
auto to_json(const trace& log) -> nlohmann::json;

/**
 * @brief Converts JSON to a trace event.
 * @param log JSON representation of the trace event.
 * @return The trace event.
 */
auto from_json(const rapidjson::Value& log) -> trace;

/**
 * @brief Writes trace events to a log file.
 * @param logs Multimap of trace events with timestamps.
 * @param file Path to the log file.
 */
void write_log_file(const std::multimap<double, trace>& logs, std::filesystem::path& file);

/**
 * @brief Reads trace events from a log file.
 * @param file Path to the log file.
 * @return Multimap of trace events with timestamps.
 */
auto read_log_file(const std::filesystem::path& file) -> std::vector<std::pair<double, trace>>;

} // namespace protocols::traces

#endif
