#pragma once

/// @file scenario_loader.hpp
/// @brief Functions and data structures for loading and writing JSON scenario files.
/// @ingroup io_loaders

#include <schedsim/core/types.hpp>

#include <cstdint>
#include <filesystem>
#include <ostream>
#include <string_view>
#include <vector>

namespace schedsim::io {

/// @brief Parameters for a single job instance within a task.
///
/// @ingroup io_loaders
/// @see TaskParams, core::TimePoint, core::Duration
struct JobParams {
    core::TimePoint arrival{};  ///< Absolute arrival time of the job.
    core::Duration duration{};  ///< Actual execution demand (at reference speed).
};

/// @brief Parameters describing a periodic real-time task.
///
/// A task produces a series of jobs according to its period.
/// The @c relative_deadline defaults to @c period when not specified in JSON.
///
/// @ingroup io_loaders
/// @see JobParams, ScenarioData
struct TaskParams {
    uint64_t id;                        ///< Unique task identifier (1-based in JSON).
    core::Duration period;              ///< Inter-arrival period.
    core::Duration relative_deadline;   ///< Relative deadline (defaults to period if unspecified).
    core::Duration wcet;                ///< Worst-case execution time.
    std::vector<JobParams> jobs;        ///< Concrete job instances to simulate.
};

/// @brief Complete scenario definition: a collection of tasks and their jobs.
///
/// Loaded from JSON via @ref load_scenario or built programmatically with
/// the generation utilities in scenario_generation.hpp.
///
/// @ingroup io_loaders
/// @see TaskParams, load_scenario, generate_scenario
struct ScenarioData {
    std::vector<TaskParams> tasks;  ///< All tasks in the scenario.
};

/// @brief Load a scenario from a JSON file.
///
/// Auto-detects the JSON format and returns a @ref ScenarioData with all
/// tasks and their pre-computed job arrivals.
///
/// @param path  Filesystem path to the JSON scenario file.
/// @return Parsed scenario data.
///
/// @throws LoaderError  If the file cannot be read or contains invalid JSON.
///
/// @see load_scenario_from_string, inject_scenario
ScenarioData load_scenario(const std::filesystem::path& path);

/// @brief Load a scenario from a JSON string.
///
/// Parses @p json directly and returns the resulting @ref ScenarioData.
///
/// @param json  JSON content describing the scenario.
/// @return Parsed scenario data.
///
/// @throws LoaderError  If the JSON is malformed or fails validation.
///
/// @see load_scenario
ScenarioData load_scenario_from_string(std::string_view json);

/// @brief Write a scenario to a JSON file.
///
/// Serialises @p scenario to the canonical JSON format and writes it to
/// the file at @p path.
///
/// @param scenario  The scenario data to serialise.
/// @param path      Destination file path.
///
/// @see write_scenario_to_stream
void write_scenario(const ScenarioData& scenario, const std::filesystem::path& path);

/// @brief Write a scenario to an output stream.
///
/// Serialises @p scenario as JSON and writes it to @p out.
///
/// @param scenario  The scenario data to serialise.
/// @param out       Output stream (file, stringstream, stdout, etc.).
///
/// @see write_scenario
void write_scenario_to_stream(const ScenarioData& scenario, std::ostream& out);

} // namespace schedsim::io
