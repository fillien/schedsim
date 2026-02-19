#pragma once

/// @file scenario_injection.hpp
/// @brief Functions for injecting scenario data into a simulation engine.
/// @ingroup io_loaders

#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/core/engine.hpp>

#include <vector>

namespace schedsim::io {

/// @brief Add tasks to the platform and schedule their job arrivals.
///
/// For each task in @p scenario, creates a corresponding @c core::Task in the
/// engine's platform and registers timed events for every job arrival.
///
/// @note Must be called **before** `Platform::finalize()`.
///
/// @param engine    The simulation engine to populate.
/// @param scenario  Scenario data containing tasks and job arrival lists.
/// @return Pointers to the created @c core::Task objects, in the same order
///         as @p scenario.tasks.
///
/// @throws LoaderError  If a task ID conflict or invalid parameter is detected.
///
/// @see load_scenario, schedule_arrivals, core::Task, core::Platform
std::vector<core::Task*> inject_scenario(core::Engine& engine, const ScenarioData& scenario);

/// @brief Schedule job arrivals for a single existing task.
///
/// Registers timed arrival events for every job in @p jobs on the given
/// @p task. Useful when tasks are created manually rather than through
/// @ref inject_scenario.
///
/// @note Must be called **after** `Platform::finalize()`.
///
/// @param engine  The simulation engine that owns the timer queue.
/// @param task    The task to receive job arrivals.
/// @param jobs    Job arrival parameters (times and durations).
///
/// @see inject_scenario, JobParams, core::Engine
void schedule_arrivals(core::Engine& engine, core::Task& task,
                       const std::vector<JobParams>& jobs);

} // namespace schedsim::io
