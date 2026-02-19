#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

/// @brief Best-Fit allocator for multi-cluster task placement.
///
/// Among all clusters that can admit the task, selects the one with the
/// lowest remaining capacity (as reported by
/// Cluster::remaining_capacity()).  Ties are broken by construction order
/// (earlier cluster wins).
///
/// Best-fit packs tasks tightly, which can consolidate work onto fewer
/// clusters and maximise opportunities for powering down idle clusters.
///
/// @ingroup algo_allocators
/// @see MultiClusterAllocator, Cluster, Allocator, EdfScheduler
class BestFitAllocator : public MultiClusterAllocator {
public:
    /// @brief Inherit MultiClusterAllocator constructors.
    using MultiClusterAllocator::MultiClusterAllocator;

protected:
    /// @brief Select the admissible cluster with the least remaining capacity.
    ///
    /// All clusters are evaluated; only those for which
    /// Cluster::can_admit() succeeds are considered.  Among candidates the
    /// one with the smallest Cluster::remaining_capacity() is chosen, with
    /// ties broken by construction order.
    ///
    /// @param task  The task requiring placement.
    /// @return Pointer to the tightest-fitting Cluster, or @c nullptr if no
    ///         cluster can accommodate the task.
    /// @see Cluster::remaining_capacity, Cluster::can_admit
    Cluster* select_cluster(const core::Task& task) override;
};

} // namespace schedsim::algo
