#include <schedsim/algo/single_scheduler_allocator.hpp>

namespace schedsim::algo {

SingleSchedulerAllocator::SingleSchedulerAllocator(core::Engine& engine, Scheduler& scheduler)
    : engine_(engine)
    , scheduler_(scheduler) {
    // Install job arrival handler on the engine
    engine_.set_job_arrival_handler(
        [this](core::Task& task, core::Job job) {
            on_job_arrival(task, std::move(job));
        });
}

void SingleSchedulerAllocator::on_job_arrival(core::Task& task, core::Job job) {
    // Route all jobs to the single scheduler
    scheduler_.on_job_arrival(task, std::move(job));
}

} // namespace schedsim::algo
