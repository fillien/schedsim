#include <schedsim/algo/multi_cluster_allocator.hpp>

#include <schedsim/algo/cluster.hpp>
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>

namespace schedsim::algo {

MultiClusterAllocator::MultiClusterAllocator(core::Engine& engine,
                                             std::vector<Cluster*> clusters)
    : engine_(engine)
    , clusters_(std::move(clusters)) {
    // Install job arrival handler on the engine (same pattern as SingleSchedulerAllocator)
    engine_.set_job_arrival_handler(
        [this](core::Task& task, core::Job job) {
            on_job_arrival(task, std::move(job));
        });
}

void MultiClusterAllocator::on_job_arrival(core::Task& task, core::Job job) {
    auto it = task_assignments_.find(&task);
    if (it != task_assignments_.end()) {
        // Subsequent jobs: route to same cluster
        it->second->scheduler().on_job_arrival(task, std::move(job));
        return;
    }

    // First job: select cluster, route (server auto-created by scheduler)
    Cluster* cluster = select_cluster(task);
    if (!cluster) {
        engine_.trace([&](core::TraceWriter& w) {
            w.type("task_rejected");
            w.field("tid", static_cast<uint64_t>(task.id()));
        });
        return;  // rejected
    }

    try {
        task_assignments_[&task] = cluster;
        engine_.trace([&](core::TraceWriter& w) {
            w.type("task_placed");
            w.field("tid", static_cast<uint64_t>(task.id()));
            w.field("cluster_id", static_cast<uint64_t>(cluster->clock_domain().id()));
            if (auto pid = cluster->processor_id()) {
                w.field("cpu", static_cast<uint64_t>(*pid));
            }
        });
        cluster->scheduler().on_job_arrival(task, std::move(job));
    } catch (const AdmissionError&) {
        // Scheduler rejected the task (capacity exceeded)
        task_assignments_.erase(&task);
        engine_.trace([&](core::TraceWriter& w) {
            w.type("task_rejected");
            w.field("tid", static_cast<uint64_t>(task.id()));
        });
    }
}

std::span<Cluster* const> MultiClusterAllocator::clusters() const noexcept {
    return clusters_;
}

} // namespace schedsim::algo
