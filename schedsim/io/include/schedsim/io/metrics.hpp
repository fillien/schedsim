#pragma once

/// @file metrics.hpp
/// @brief Post-simulation metrics computation and time-series extraction.
///
/// Defines data structures for aggregated simulation statistics (deadlines,
/// energy, response times, migrations) and functions that derive them from
/// in-memory or on-disk traces. Also provides time-series interval tracking
/// for frequency, core-count, and combined configuration changes.
///
/// @ingroup io_metrics

#include <schedsim/io/trace_writers.hpp>

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace schedsim::io {

/// @brief Aggregated metrics computed from a simulation trace.
///
/// Covers scheduling quality (deadline misses, preemptions), energy
/// consumption, per-processor utilization, per-task response/waiting times,
/// frequency changes, and migration/transition counts.
///
/// @ingroup io_metrics
/// @see compute_metrics, compute_metrics_from_file
struct SimulationMetrics {
    // -- Scheduling metrics --------------------------------------------------

    uint64_t total_jobs{0};        ///< Total number of job arrivals.
    uint64_t completed_jobs{0};    ///< Jobs that finished before their deadline.
    uint64_t deadline_misses{0};   ///< Jobs that missed their absolute deadline.
    uint64_t preemptions{0};       ///< Number of preemption events.
    uint64_t context_switches{0};  ///< Number of context-switch events.

    // -- Energy metrics ------------------------------------------------------

    double total_energy_mj{0.0};  ///< Total energy consumed (millijoules).
    /// @brief Per-processor energy consumption (processor ID -> millijoules).
    std::unordered_map<uint64_t, double> energy_per_processor;

    // -- Utilization ---------------------------------------------------------

    double average_utilization{0.0};  ///< System-wide average utilization [0, 1].
    /// @brief Per-processor average utilization (processor ID -> utilization).
    std::unordered_map<uint64_t, double> utilization_per_processor;

    // -- Response times ------------------------------------------------------

    /// @brief Per-task response times in seconds (task ID -> list of values).
    /// @see compute_response_time_stats
    std::unordered_map<uint64_t, std::vector<double>> response_times_per_task;

    // -- Deadline misses per task --------------------------------------------

    /// @brief Per-task deadline miss counts (task ID -> count).
    std::unordered_map<uint64_t, uint64_t> deadline_misses_per_task;

    // -- Rejected tasks ------------------------------------------------------

    uint64_t rejected_tasks{0};  ///< Number of tasks rejected by the admission test.

    // -- Waiting times -------------------------------------------------------

    /// @brief Per-task waiting times in seconds (task ID -> list of arrival-to-start delays).
    std::unordered_map<uint64_t, std::vector<double>> waiting_times_per_task;

    // -- Frequency changes ---------------------------------------------------

    /// @brief Single frequency-change event recorded during simulation.
    struct FrequencyChange {
        double time;          ///< Simulation time of the change (seconds).
        uint64_t cluster_id;  ///< Clock-domain / cluster identifier.
        double frequency;     ///< New frequency after the change (Hz).
    };

    /// @brief Chronological log of all frequency-change events.
    std::vector<FrequencyChange> frequency_changes;

    // -- Migration and transition counts -------------------------------------

    uint64_t cluster_migrations{0};   ///< Number of inter-cluster migration events.
    uint64_t transitions{0};          ///< Number of task-placement events.
    uint64_t core_state_requests{0};  ///< Processor activation/sleep transitions.
    uint64_t frequency_requests{0};   ///< Distinct frequency-change timestamps.
};

/// @brief Compute aggregated metrics from in-memory trace records.
///
/// Iterates over @p traces and accumulates scheduling, energy, utilization,
/// response-time, and migration statistics.
///
/// @param traces  Vector of trace records (typically from MemoryTraceWriter).
/// @return Populated SimulationMetrics.
///
/// @see compute_metrics_from_file, MemoryTraceWriter
SimulationMetrics compute_metrics(const std::vector<TraceRecord>& traces);

/// @brief Compute aggregated metrics from a JSON trace file on disk.
///
/// Opens the file at @p path, parses its trace records, and returns the
/// same metrics as compute_metrics.
///
/// @param path  Filesystem path to a JSON trace file.
/// @return Populated SimulationMetrics.
///
/// @throws LoaderError  If the file cannot be read or parsed.
///
/// @see compute_metrics, JsonTraceWriter
SimulationMetrics compute_metrics_from_file(const std::filesystem::path& path);

/// @brief Summary statistics for a collection of response times.
///
/// @ingroup io_metrics
/// @see compute_response_time_stats
struct ResponseTimeStats {
    double min{0.0};             ///< Minimum response time.
    double max{0.0};             ///< Maximum response time.
    double mean{0.0};            ///< Arithmetic mean.
    double median{0.0};          ///< Median (50th percentile).
    double stddev{0.0};          ///< Standard deviation.
    double percentile_95{0.0};   ///< 95th percentile.
    double percentile_99{0.0};   ///< 99th percentile.
};

/// @brief Compute summary statistics from a vector of response times.
///
/// @param response_times  Response times in seconds (must not be empty for
///                        meaningful results).
/// @return A ResponseTimeStats populated with min/max/mean/median/stddev
///         and tail percentiles.
///
ResponseTimeStats compute_response_time_stats(const std::vector<double>& response_times);

// ---------------------------------------------------------------------------
// Time-series interval tracking
// ---------------------------------------------------------------------------

/// @brief A time interval during which a clock domain held a constant frequency.
///
/// @ingroup io_metrics
/// @see track_frequency_changes
struct FrequencyInterval {
    double start;         ///< Start time of the interval (seconds).
    double stop;          ///< End time of the interval (seconds).
    double frequency;     ///< Frequency during this interval (Hz).
    uint64_t cluster_id;  ///< Clock-domain / cluster identifier.
};

/// @brief A time interval during which a cluster had a constant active-core count.
///
/// @ingroup io_metrics
/// @see track_core_changes
struct CoreCountInterval {
    double start;           ///< Start time of the interval (seconds).
    double stop;            ///< End time of the interval (seconds).
    uint64_t active_cores;  ///< Number of active cores during this interval.
    uint64_t cluster_id;    ///< Clock-domain / cluster identifier.
};

/// @brief A time interval during which the global configuration (frequency +
///        active cores) remained constant.
///
/// @ingroup io_metrics
/// @see track_config_changes
struct ConfigInterval {
    double start;           ///< Start time of the interval (seconds).
    double stop;            ///< End time of the interval (seconds).
    double frequency;       ///< Frequency during this interval (Hz).
    uint64_t active_cores;  ///< Number of active cores during this interval.
};

/// @brief Extract per-cluster frequency intervals from a trace.
///
/// Scans @p traces for frequency-change events and returns a sorted vector
/// of intervals, one per cluster per constant-frequency span.
///
/// @param traces  In-memory trace records.
/// @return Sorted vector of FrequencyInterval entries.
///
/// @see FrequencyInterval, track_core_changes, track_config_changes
std::vector<FrequencyInterval> track_frequency_changes(
    const std::vector<TraceRecord>& traces);

/// @brief Extract per-cluster active-core-count intervals from a trace.
///
/// Scans @p traces for core activation/sleep events and returns a sorted
/// vector of intervals.
///
/// @param traces  In-memory trace records.
/// @return Sorted vector of CoreCountInterval entries.
///
/// @see CoreCountInterval, track_frequency_changes, track_config_changes
std::vector<CoreCountInterval> track_core_changes(
    const std::vector<TraceRecord>& traces);

/// @brief Extract combined configuration intervals from a trace.
///
/// Merges frequency and core-count changes into intervals where neither
/// changed, giving a unified view of the system configuration over time.
///
/// @param traces  In-memory trace records.
/// @return Sorted vector of ConfigInterval entries.
///
/// @see ConfigInterval, track_frequency_changes, track_core_changes
std::vector<ConfigInterval> track_config_changes(
    const std::vector<TraceRecord>& traces);

} // namespace schedsim::io
