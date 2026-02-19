#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

/// @brief First-fit allocator with capacity-aware admission control.
/// @ingroup algo_allocators
///
/// Extends the first-fit strategy with an admission test that checks whether
/// a cluster has enough remaining utilisation bandwidth to accommodate the
/// incoming task.  Tasks are rejected (returned as @c nullptr) when no
/// cluster can satisfy the capacity constraint.
///
/// @see MultiClusterAllocator for the base class and task-binding semantics.
/// @see FFBigFirstAllocator for a simpler first-fit without capacity checks.
/// @see FFCapAdaptiveLinearAllocator for an adaptive variant with linear thresholds.
/// @see FFCapAdaptivePolyAllocator for an adaptive variant with polynomial thresholds.
class FFCapAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

protected:
    /// @brief Select the first cluster whose residual capacity can accommodate the task.
    /// @param task The task to be allocated.
    /// @return Pointer to the chosen Cluster, or @c nullptr if no cluster has sufficient capacity.
    /// @see MultiClusterAllocator::select_cluster
    Cluster* select_cluster(const core::Task& task) override;
};

} // namespace schedsim::algo
