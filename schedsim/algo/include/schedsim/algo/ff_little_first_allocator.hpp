#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

#include <cstddef>

namespace schedsim::algo {

/// @brief First-fit allocator that processes little (energy-efficient) cores first.
/// @ingroup algo_allocators
///
/// Iterates over clusters from lowest performance to highest, assigning each
/// task to the first cluster that can accept it.  This energy-oriented strategy
/// uses small cores whenever possible, falling back to big cores only when
/// capacity is exhausted.  Tracks the total number of allocation steps.
///
/// @see MultiClusterAllocator for the base class and task-binding semantics.
/// @see FFBigFirstAllocator for the performance-oriented counterpart.
class FFLittleFirstAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

    /// @brief Return the number of allocation steps performed so far.
    /// @return The cumulative count of select_cluster invocations.
    [[nodiscard]] std::size_t allocation_count() const noexcept { return step_; }

protected:
    /// @brief Select the first cluster with sufficient capacity, starting from little cores.
    /// @param task The task to be allocated.
    /// @return Pointer to the chosen Cluster, or @c nullptr if no cluster can accept the task.
    /// @see MultiClusterAllocator::select_cluster
    Cluster* select_cluster(const core::Task& task) override;

private:
    /// @brief Running count of allocation attempts.
    std::size_t step_{0};
};

} // namespace schedsim::algo
