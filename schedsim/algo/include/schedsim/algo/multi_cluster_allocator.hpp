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

// Base allocator for multi-cluster systems.
// Routes jobs to clusters via a virtual select_cluster() method.
// Task binding is permanent: once assigned, a task always goes to the same cluster.
class MultiClusterAllocator : public Allocator {
public:
    MultiClusterAllocator(core::Engine& engine, std::vector<Cluster*> clusters);
    ~MultiClusterAllocator() override = default;

    // Non-copyable, non-movable (registered with engine)
    MultiClusterAllocator(const MultiClusterAllocator&) = delete;
    MultiClusterAllocator& operator=(const MultiClusterAllocator&) = delete;
    MultiClusterAllocator(MultiClusterAllocator&&) = delete;
    MultiClusterAllocator& operator=(MultiClusterAllocator&&) = delete;

    void on_job_arrival(core::Task& task, core::Job job) override;

protected:
    // Override point: select which cluster should handle this task.
    // Return nullptr to reject the task (no suitable cluster found).
    virtual Cluster* select_cluster(const core::Task& task) = 0;

    [[nodiscard]] std::span<Cluster* const> clusters() const noexcept;

private:
    core::Engine& engine_;
    std::vector<Cluster*> clusters_;
    std::unordered_map<const core::Task*, Cluster*> task_assignments_;
};

} // namespace schedsim::algo
