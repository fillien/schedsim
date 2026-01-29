#pragma once

#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/core/engine.hpp>

namespace schedsim::io {

// Add tasks to platform and schedule job arrivals
// Must be called BEFORE platform.finalize()
// Returns a mapping from scenario task IDs to actual Task references
void inject_scenario(core::Engine& engine, const ScenarioData& scenario);

// Schedule job arrivals for an existing task
// Must be called AFTER platform.finalize()
void schedule_arrivals(core::Engine& engine, core::Task& task,
                       const std::vector<JobParams>& jobs);

} // namespace schedsim::io
