#include <schedsim/io/scenario_injection.hpp>

#include <schedsim/core/platform.hpp>
#include <schedsim/core/task.hpp>

namespace schedsim::io {

std::vector<core::Task*> inject_scenario(core::Engine& engine, const ScenarioData& scenario) {
    auto& platform = engine.platform();
    std::vector<core::Task*> tasks;
    tasks.reserve(scenario.tasks.size());

    for (const auto& task_params : scenario.tasks) {
        auto& task = platform.add_task(
            task_params.id,
            task_params.period,
            task_params.relative_deadline,
            task_params.wcet);
        tasks.push_back(&task);
    }
    return tasks;
}

void schedule_arrivals(core::Engine& engine, core::Task& task,
                       const std::vector<JobParams>& jobs) {
    for (const auto& job : jobs) {
        engine.schedule_job_arrival(task, job.arrival, job.duration);
    }
}

} // namespace schedsim::io
