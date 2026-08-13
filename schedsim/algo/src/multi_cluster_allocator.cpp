#include <schedsim/algo/multi_cluster_allocator.hpp>

#include <schedsim/algo/cluster.hpp>
#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/scheduler.hpp>
#include <schedsim/algo/task_utils.hpp>

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
        if (!migration_enabled_) {
            // Original behavior: forward to cached cluster
            it->second->scheduler().on_job_arrival(task, std::move(job));
            return;
        }

        // --- Migration re-evaluation ---
        Cluster* old_cluster = it->second;
        auto& old_edf = static_cast<EdfScheduler&>(old_cluster->scheduler());
        CbsServer* server = old_edf.find_server(task);

        if (!server) {
            // Server was detached (M-GRUB) — forward to old cluster
            old_cluster->scheduler().on_job_arrival(task, std::move(job));
            return;
        }

        auto state = server->state();

        // Only migrate from Inactive or NonContending
        if (state != CbsServer::State::Inactive &&
            state != CbsServer::State::NonContending) {
            old_cluster->scheduler().on_job_arrival(task, std::move(job));
            return;
        }

        double task_util = task_utilization(task);
        double server_util = server->utilization();

        // Temporarily unadmit for fair re-evaluation
        if (tracks_scaled_utilization()) {
            old_cluster->unadmit_scaled(task_util);
        }
        old_edf.adjust_utilization(-server_util);

        // Re-evaluate placement
        Cluster* new_cluster = select_cluster(task);

        if (!new_cluster) {
            // All clusters rejected — rollback
            if (tracks_scaled_utilization()) {
                old_cluster->readmit_scaled(task_util);
            }
            old_edf.adjust_utilization(server_util);
            old_cluster->scheduler().on_job_arrival(task, std::move(job));
            return;
        }

        if (new_cluster == old_cluster) {
            // Same cluster — FFCap's try_admit_scaled already re-admitted, don't re-admit
            old_edf.adjust_utilization(server_util);
            old_cluster->scheduler().on_job_arrival(task, std::move(job));
            return;
        }

        // Different cluster — pre-check new scheduler admission before committing
        auto& new_edf = static_cast<EdfScheduler&>(new_cluster->scheduler());
        if (!new_edf.can_admit(task.wcet(), task.period())) {
            // New scheduler would reject — rollback
            if (tracks_scaled_utilization()) {
                new_cluster->unadmit_scaled(task_util);
                old_cluster->readmit_scaled(task_util);
            }
            old_edf.adjust_utilization(server_util);
            old_cluster->scheduler().on_job_arrival(task, std::move(job));
            return;
        }

        // --- Migration confirmed ---
        //
        // Asymmetric tracking note:
        // - Cluster-level total_scaled_utilization_: already unadmitted from old_cluster
        //   in the evaluation step above, and stays unadmitted (task is leaving old_cluster).
        //   select_cluster() called try_admit_scaled on new_cluster, so new_cluster is
        //   already charged.
        // - Scheduler-level total_utilization_: restored here via adjust_utilization.
        //   For Inactive: it will be decremented again by remove_inactive_server below.
        //   For NonContending: the zombie must stay counted for GRUB accounting until
        //   its deadline fires, at which point try_detach_server decrements it.

        // Restore old scheduler utilization
        old_edf.adjust_utilization(server_util);

        // Commit irreversible operations on old scheduler
        if (state == CbsServer::State::Inactive) {
            old_edf.remove_inactive_server(task);
        } else {
            // NonContending: zombie migration
            old_edf.detach_task_mapping(task);
            server->set_pending_removal([]() {});
        }

        // Update assignment
        task_assignments_[&task] = new_cluster;

        // Transfer M-GRUB expected arrivals to new scheduler
        auto expected = old_edf.get_expected_arrivals(task);
        if (expected) {
            std::size_t count = old_edf.get_arrival_count(task);
            if (*expected > count) {
                new_edf.set_expected_arrivals(task, *expected - count);
            }
        }

        // Trace migration event
        engine_.trace([&](core::TraceWriter& w) {
            w.type("migration_cluster");
            w.field("tid", static_cast<uint64_t>(task.id()));
            w.field("old_cluster_id", static_cast<uint64_t>(old_cluster->clock_domain().id()));
            w.field("new_cluster_id", static_cast<uint64_t>(new_cluster->clock_domain().id()));
        });

        // Forward job to new cluster
        new_cluster->scheduler().on_job_arrival(task, std::move(job));
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
        if (tracks_scaled_utilization()) {
            cluster->unadmit_scaled(task_utilization(task));
        }
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
