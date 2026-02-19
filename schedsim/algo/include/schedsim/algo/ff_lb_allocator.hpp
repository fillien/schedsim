#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

/// @brief First-fit allocator with load-balancing across clusters.
/// @ingroup algo_allocators
///
/// Instead of always scanning clusters in a fixed order, this allocator
/// selects the cluster with the lowest current utilisation that can still
/// accommodate the incoming task.  This spreads the load more evenly across
/// clusters than a pure first-fit strategy, reducing worst-case per-cluster
/// congestion.
///
/// @see MultiClusterAllocator for the base class and task-binding semantics.
/// @see FFBigFirstAllocator for fixed big-first ordering.
/// @see FFLittleFirstAllocator for fixed little-first ordering.
/// @see FFCapAllocator for capacity-aware admission without load balancing.
class FFLbAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

protected:
    /// @brief Select the least-loaded cluster that can accommodate the task.
    /// @param task The task to be allocated.
    /// @return Pointer to the chosen Cluster, or @c nullptr if no cluster can accept the task.
    /// @see MultiClusterAllocator::select_cluster
    Cluster* select_cluster(const core::Task& task) override;
};

} // namespace schedsim::algo
