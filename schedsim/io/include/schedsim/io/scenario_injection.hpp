#pragma once

#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/core/engine.hpp>

#include <vector>

namespace schedsim::io {

// Add tasks to platform and schedule job arrivals
// Must be called BEFORE platform.finalize()
// Returns pointers to the created tasks in the same order as scenario.tasks
std::vector<core::Task*> inject_scenario(core::Engine& engine, const ScenarioData& scenario);

// Schedule job arrivals for an existing task
// Must be called AFTER platform.finalize()
void schedule_arrivals(core::Engine& engine, core::Task& task,
                       const std::vector<JobParams>& jobs);

} // namespace schedsim::io
