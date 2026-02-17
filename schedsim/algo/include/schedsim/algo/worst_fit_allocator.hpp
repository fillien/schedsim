#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

// Worst-Fit allocator: picks the cluster with the highest remaining capacity
// among those that can admit the task. Ties broken by construction order.
class WorstFitAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

protected:
    Cluster* select_cluster(const core::Task& task) override;
};

} // namespace schedsim::algo
