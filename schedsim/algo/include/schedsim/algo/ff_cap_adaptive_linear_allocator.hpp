#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

/// @brief Capacity-aware first-fit allocator with a linearly adaptive admission threshold.
/// @ingroup algo_allocators
///
/// Builds on FFCapAllocator by dynamically adjusting the admission threshold
/// using a linear function of the observed maximum per-task utilisation and the
/// expected total task-set utilisation.  The threshold tightens as the
/// allocator gathers more information about incoming workloads.
///
/// @see MultiClusterAllocator for the base class and task-binding semantics.
/// @see FFCapAllocator for the non-adaptive capacity-aware variant.
/// @see FFCapAdaptivePolyAllocator for a polynomial adaptive variant.
class FFCapAdaptiveLinearAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

    /// @brief Set the expected total utilisation of the task set.
    /// @param util Expected aggregate utilisation (sum of task utilisations).
    ///
    /// This value is used together with the observed maximum per-task
    /// utilisation to compute the adaptive admission threshold.
    void set_expected_total_util(double util) noexcept { expected_total_util_ = util; }

protected:
    /// @brief Select a cluster using a linearly adaptive capacity threshold.
    /// @param task The task to be allocated.
    /// @return Pointer to the chosen Cluster, or @c nullptr if no cluster passes the adaptive test.
    /// @see MultiClusterAllocator::select_cluster
    Cluster* select_cluster(const core::Task& task) override;

private:
    /// @brief Highest per-task utilisation observed so far.
    double observed_umax_{0.0};
    /// @brief Expected total utilisation supplied by the caller.
    double expected_total_util_{0.0};
};

} // namespace schedsim::algo
