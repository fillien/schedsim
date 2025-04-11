#ifndef TRACES_HPP
#define TRACES_HPP

#include <cstddef>
#include <filesystem>
#include <map>
#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <variant>
#include <vector>

namespace protocols::traces {

/**
 * @brief Represents a rescheduling event.
 */
struct Resched {};

/**
 * @brief Represents a simulation finished event.
 */
struct SimFinished {};

/**
 * @brief Represents a job arrival event.
 */
struct JobArrival {
        std::size_t task_id; /**< ID of the task. */
        double duration;     /**< Duration of the job. */
        double deadline;     /**< Absolut deadline of the job. */
};

/**
 * @brief Represents a job finished event.
 */
struct JobFinished {
        std::size_t task_id; /**< ID of the task. */
};

/**
 * @brief Represents a processor activated event.
 */
struct ProcActivated {
        std::size_t proc_id;    /**< ID of the processor. */
        std::size_t cluster_id; /**< ID of the cluster. */
};

/**
 * @brief Represents a processor idled event.
 */
struct ProcIdled {
        std::size_t proc_id;    /**< ID of the processor. */
        std::size_t cluster_id; /**< ID of the cluster. */
};

/**
 * @brief Represents a processor activated event.
 */
struct ProcSleep {
        std::size_t proc_id;    /**< ID of the processor. */
        std::size_t cluster_id; /**< ID of the cluster. */
};

/**
 * @brief Represents a processor change event.
 */
struct ProcChange {
        std::size_t proc_id;    /**< ID of the processor. */
        std::size_t cluster_id; /**< ID of the cluster. */
};

/**
 * @brief Represents a service budget replenished event.
 */
struct ServBudgetReplenished {
        std::size_t sched_id; /** ID of the scheduler */
        std::size_t task_id;  /**< ID of the task. */
        double budget;        /**< Replenished budget. */
};

/**
 * @brief Represents a service inactive event.
 */
struct ServInactive {
        std::size_t sched_id; /** ID of the scheduler */
        std::size_t task_id;  /**< ID of the task. */
        double utilization;   /**< Utilization of the task. */
};

/**
 * @brief Represents a service budget exhausted event.
 */
struct ServBudgetExhausted {
        std::size_t sched_id; /** ID of the scheduler */
        std::size_t task_id;  /**< ID of the task. */
};

/**
 * @brief Represents a non-continuous service event.
 */
struct ServNonCont {
        std::size_t sched_id; /** ID of the scheduler */
        std::size_t task_id;  /**< ID of the task. */
};

/**
 * @brief Represents a service postpone event.
 */
struct ServPostpone {
        std::size_t sched_id; /** ID of the scheduler */
        std::size_t task_id;  /**< ID of the task. */
        double deadline;      /**< New deadline after postponement. */
};

/**
 * @brief Represents a service ready event.
 */
struct ServReady {
        std::size_t sched_id; /** ID of the scheduler */
        std::size_t task_id;  /**< ID of the task. */
        double deadline;      /**< Deadline of the task. */
        double utilization;   /**< Utilization of the task. */
};

/**
 * @brief Represents a service running event.
 */
struct ServRunning {
        std::size_t sched_id; /** ID of the scheduler */
        std::size_t task_id;  /**< ID of the task. */
};

/**
 * @brief Represents a task preempted event.
 */
struct TaskPreempted {
        std::size_t task_id; /**< ID of the task. */
};

/**
 * @brief Represents a task scheduled event.
 */
struct TaskScheduled {
        std::size_t task_id; /**< ID of the task. */
        std::size_t proc_id; /**< ID of the processor. */
};

/**
 * @brief Represents a task rejected event.
 */
struct TaskRejected {
        std::size_t task_id; /**< ID of the task. */
};

/**
 * @brief Represents a virtual time update event.
 */
struct VirtualTimeUpdate {
        std::size_t task_id; /**< ID of the task. */
        double virtual_time; /**< Updated virtual time. */
};

/**
 * @brief Represents a frequency scaling of the platform
 */
struct FrequencyUpdate {
        std::size_t cluster_id; /**< ID of the cluster. */
        double frequency;       /**< New frequency. */
};

struct TaskPlaced {
        std::size_t task_id;
        std::size_t cluster_id;
};

struct MigrationCluster {
        std::size_t task_id;
        std::size_t cluster_id;
};

/**
 * @brief Type representing various trace events.
 */
using trace = std::variant<
    Resched,
    SimFinished,
    VirtualTimeUpdate,
    FrequencyUpdate,
    JobArrival,
    JobFinished,
    ProcActivated,
    ProcIdled,
    ProcSleep,
    ProcChange,
    ServBudgetExhausted,
    ServInactive,
    ServBudgetReplenished,
    ServNonCont,
    ServPostpone,
    ServReady,
    ServRunning,
    TaskPreempted,
    TaskScheduled,
    TaskRejected,
    TaskPlaced,
    MigrationCluster>;

/**
 * @brief Converts a trace event to JSON format.
 * @param log The trace event to convert.
 * @return JSON representation of the trace event.
 */
void to_json(const trace& log, rapidjson::Writer<rapidjson::OStreamWrapper>& writer);

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
void write_log_file(const std::multimap<double, trace>& logs, const std::filesystem::path& file);

/**
 * @brief Reads trace events from a log file.
 * @param file Path to the log file.
 * @return Multimap of trace events with timestamps.
 */
auto read_log_file(const std::filesystem::path& file) -> std::vector<std::pair<double, trace>>;

} // namespace protocols::traces

#endif