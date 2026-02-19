#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

#include <cstddef>

namespace schedsim::algo {

/// @brief Allocator that counts the number of allocation attempts.
/// @ingroup algo_allocators
///
/// Wraps a trivial allocation strategy while maintaining a running count
/// of how many times select_cluster has been invoked.  Useful for
/// instrumentation, testing, and comparing the decision frequency of
/// different allocation policies.
///
/// @see MultiClusterAllocator for the base class and task-binding semantics.
class CountingAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

    /// @brief Return the number of allocation attempts performed so far.
    /// @return The cumulative count of select_cluster invocations.
    [[nodiscard]] std::size_t allocation_count() const noexcept { return count_; }

protected:
    /// @brief Select a cluster for the task and increment the internal counter.
    /// @param task The task to be allocated.
    /// @return Pointer to the chosen Cluster, or @c nullptr if no cluster can accept the task.
    /// @see MultiClusterAllocator::select_cluster
    Cluster* select_cluster(const core::Task& task) override;

private:
    /// @brief Running count of allocation attempts.
    std::size_t count_{0};
};

} // namespace schedsim::algo
