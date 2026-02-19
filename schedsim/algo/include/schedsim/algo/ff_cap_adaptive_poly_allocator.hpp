#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

/// @brief Capacity-aware first-fit allocator with a polynomially adaptive admission threshold.
/// @ingroup algo_allocators
///
/// Similar to FFCapAdaptiveLinearAllocator, but uses a polynomial function
/// of the observed maximum per-task utilisation and the expected total
/// utilisation to determine the admission threshold.  The polynomial model
/// can capture non-linear capacity margins that appear in heterogeneous
/// platforms.
///
/// @see MultiClusterAllocator for the base class and task-binding semantics.
/// @see FFCapAllocator for the non-adaptive capacity-aware variant.
/// @see FFCapAdaptiveLinearAllocator for the linear adaptive variant.
class FFCapAdaptivePolyAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

    /// @brief Set the expected total utilisation of the task set.
    /// @param util Expected aggregate utilisation (sum of task utilisations).
    ///
    /// This value is used together with the observed maximum per-task
    /// utilisation to compute the polynomial adaptive admission threshold.
    void set_expected_total_util(double util) noexcept { expected_total_util_ = util; }

protected:
    /// @brief Select a cluster using a polynomially adaptive capacity threshold.
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
