#include <schedsim/io/scenario_injection.hpp>

#include <schedsim/core/platform.hpp>
#include <schedsim/core/task.hpp>

namespace schedsim::io {

void inject_scenario(core::Engine& engine, const ScenarioData& scenario) {
    auto& platform = engine.platform();

    // Create tasks in platform
    // Note: We rely on the task order matching the scenario order
    // since Platform assigns sequential IDs starting from 0
    for (const auto& task_params : scenario.tasks) {
        platform.add_task(
            task_params.period,
            task_params.relative_deadline,
            task_params.wcet);
    }
}

void schedule_arrivals(core::Engine& engine, core::Task& task,
                       const std::vector<JobParams>& jobs) {
    for (const auto& job : jobs) {
        engine.schedule_job_arrival(task, job.arrival, job.duration);
    }
}

} // namespace schedsim::io
