#include <schedsim/algo/single_scheduler_allocator.hpp>

#include <schedsim/core/clock_domain.hpp>

namespace schedsim::algo {

SingleSchedulerAllocator::SingleSchedulerAllocator(core::Engine& engine, Scheduler& scheduler,
                                                   core::ClockDomain* clock_domain)
    : engine_(engine)
    , scheduler_(scheduler)
    , clock_domain_(clock_domain) {
    // Install job arrival handler on the engine
    engine_.set_job_arrival_handler(
        [this](core::Task& task, core::Job job) {
            on_job_arrival(task, std::move(job));
        });
}

void SingleSchedulerAllocator::on_job_arrival(core::Task& task, core::Job job) {
    // Emit task_placed on first job arrival for a task
    if (placed_tasks_.find(&task) == placed_tasks_.end()) {
        placed_tasks_.insert(&task);
        engine_.trace([&](core::TraceWriter& w) {
            w.type("task_placed");
            w.field("tid", static_cast<uint64_t>(task.id()));
            if (clock_domain_) {
                w.field("cluster_id", static_cast<uint64_t>(clock_domain_->id()));
            }
        });
    }

    // Route all jobs to the single scheduler
    scheduler_.on_job_arrival(task, std::move(job));
}

} // namespace schedsim::algo
