#pragma once

#include <schedsim/core/types.hpp>

#include <cstdint>
#include <filesystem>
#include <ostream>
#include <string_view>
#include <vector>

namespace schedsim::io {

struct JobParams {
    core::TimePoint arrival{};
    core::Duration duration{};
};

struct TaskParams {
    uint64_t id;
    core::Duration period;
    core::Duration relative_deadline;  // defaults to period if not specified
    core::Duration wcet;
    std::vector<JobParams> jobs;
};

struct ScenarioData {
    std::vector<TaskParams> tasks;
};

// Load scenario from JSON file (auto-detects format)
ScenarioData load_scenario(const std::filesystem::path& path);

// Load scenario from JSON string
ScenarioData load_scenario_from_string(std::string_view json);

// Write scenario to JSON file
void write_scenario(const ScenarioData& scenario, const std::filesystem::path& path);

// Write scenario to stream
void write_scenario_to_stream(const ScenarioData& scenario, std::ostream& out);

} // namespace schedsim::io
