#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

/// @brief First-Fit Decreasing allocator for multi-cluster task placement.
///
/// Iterates over the clusters in construction order and returns the first
/// cluster that can admit the task (i.e. whose Cluster::can_admit() returns
/// true for the task's budget and period).
///
/// This is the classic first-fit bin-packing heuristic.  It favours
/// filling earlier clusters first, which tends to leave later clusters
/// idle and available for power management.
///
/// @ingroup algo_allocators
/// @see MultiClusterAllocator, Cluster, Allocator, EdfScheduler
class FirstFitAllocator : public MultiClusterAllocator {
public:
    /// @cond INTERNAL
    using MultiClusterAllocator::MultiClusterAllocator;
    /// @endcond

protected:
    /// @brief Select the first cluster that can admit the task.
    ///
    /// Clusters are probed in construction order.  The first one for which
    /// Cluster::can_admit() succeeds is returned.
    ///
    /// @param task  The task requiring placement.
    /// @return Pointer to the first admissible Cluster, or @c nullptr if no
    ///         cluster can accommodate the task.
    /// @see Cluster::can_admit, Cluster::scaled_utilization
    Cluster* select_cluster(const core::Task& task) override;
};

} // namespace schedsim::algo
