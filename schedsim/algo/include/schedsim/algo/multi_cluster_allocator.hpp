#pragma once

#include <schedsim/algo/allocator.hpp>
#include <schedsim/algo/cluster.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>

#include <span>
#include <unordered_map>
#include <vector>

namespace schedsim::algo {

/// @brief Base allocator for multi-cluster systems.
///
/// Routes incoming jobs to one of several Cluster instances via the virtual
/// select_cluster() hook.  Task binding is permanent: once a task has been
/// assigned to a cluster by select_cluster(), every subsequent job of that
/// task is forwarded to the same cluster without re-invoking the selection
/// logic.
///
/// Concrete subclasses (FirstFitAllocator, WorstFitAllocator,
/// BestFitAllocator) implement different bin-packing heuristics by
/// overriding select_cluster().
///
/// @ingroup algo_allocators
/// @see Allocator, Cluster, Scheduler, EdfScheduler
class MultiClusterAllocator : public Allocator {
public:
    /// @brief Construct a multi-cluster allocator.
    /// @param engine  Reference to the simulation engine (for event scheduling).
    /// @param clusters  Non-owning pointers to the clusters available for
    ///                   task placement.  Order matters for tie-breaking in
    ///                   derived allocators.
    MultiClusterAllocator(core::Engine& engine, std::vector<Cluster*> clusters);

    /// @brief Virtual destructor.
    ~MultiClusterAllocator() override = default;

    /// Non-copyable, non-movable (registered with engine)
    MultiClusterAllocator(const MultiClusterAllocator&) = delete;
    MultiClusterAllocator& operator=(const MultiClusterAllocator&) = delete;
    MultiClusterAllocator(MultiClusterAllocator&&) = delete;
    MultiClusterAllocator& operator=(MultiClusterAllocator&&) = delete;

    /// @brief Handle a job arrival for a task.
    ///
    /// If the task has already been assigned to a cluster, the job is
    /// forwarded directly.  Otherwise select_cluster() is called to
    /// choose a cluster, and the assignment is recorded permanently.
    ///
    /// When migration is enabled, select_cluster() is called on every
    /// arrival. If a different cluster is selected and the server is
    /// in a migratable state (Inactive or NonContending), the task
    /// moves to the new cluster.
    ///
    /// @param task  The task that produced the job.
    /// @param job   The newly arrived job to dispatch.
    /// @see Allocator::on_job_arrival
    void on_job_arrival(core::Task& task, core::Job job) override;

    /// @brief Enable task migration on subsequent job arrivals.
    ///
    /// When enabled, each job arrival re-evaluates cluster placement
    /// via select_cluster(). If a better cluster is found and the
    /// server is Inactive or NonContending, the task migrates.
    void enable_migration() noexcept { migration_enabled_ = true; }

protected:
    /// @brief Select which cluster should handle a task (pure virtual).
    ///
    /// Derived classes implement their bin-packing heuristic here.
    /// Called on the first job arrival for a task, and on every
    /// subsequent arrival when migration is enabled.
    ///
    /// @param task  The task requiring placement.
    /// @return Pointer to the chosen Cluster, or @c nullptr to reject the
    ///         task (no suitable cluster found).
    /// @see Cluster::can_admit, Cluster::scaled_utilization
    virtual Cluster* select_cluster(const core::Task& task) = 0;

    /// @brief Access the list of clusters.
    /// @return A read-only span over the cluster pointers provided at
    ///         construction time.
    [[nodiscard]] std::span<Cluster* const> clusters() const noexcept;

    /// @brief Whether this allocator uses cluster-level scaled utilization tracking.
    ///
    /// When true, migration calls unadmit_scaled() / try_admit_scaled() on
    /// clusters. Override to return true in allocators that use try_admit_scaled()
    /// (e.g., FFCapAllocator). Default is false.
    [[nodiscard]] virtual bool tracks_scaled_utilization() const noexcept { return false; }

    /// @brief Access the simulation engine.
    [[nodiscard]] core::Engine& engine() noexcept { return engine_; }

    bool migration_enabled_{false};

private:
    /// @brief Remove completed tasks from bookkeeping.
    ///
    /// Scans task_assignments_ for tasks whose CBS server is Inactive with
    /// no pending jobs (or detached). For allocators that track scaled
    /// utilization, calls unadmit_scaled() to free cluster capacity.
    ///
    /// @param exclude  Task to skip (the one currently receiving a job).
    void cleanup_completed_tasks(const core::Task* exclude);

    core::Engine& engine_;
    std::vector<Cluster*> clusters_;
    std::unordered_map<const core::Task*, Cluster*> task_assignments_;
};

} // namespace schedsim::algo
