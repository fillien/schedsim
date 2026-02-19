#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

/// @brief First-fit allocator that processes big (high-performance) cores first.
/// @ingroup algo_allocators
///
/// Iterates over clusters from highest performance to lowest, assigning each
/// task to the first cluster that can accept it.  This strategy maximises
/// single-task throughput at the cost of higher energy consumption.
///
/// @see MultiClusterAllocator for the base class and task-binding semantics.
/// @see FFLittleFirstAllocator for the energy-oriented counterpart.
/// @see FFCapAllocator for a capacity-aware variant.
class FFBigFirstAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

protected:
    /// @brief Select the first cluster with sufficient capacity, starting from big cores.
    /// @param task The task to be allocated.
    /// @return Pointer to the chosen Cluster, or @c nullptr if no cluster can accept the task.
    /// @see MultiClusterAllocator::select_cluster
    Cluster* select_cluster(const core::Task& task) override;
};

} // namespace schedsim::algo
