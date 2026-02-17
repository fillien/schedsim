#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

// First-Fit allocator: iterates clusters in construction order,
// returns the first cluster that can admit the task.
class FirstFitAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

protected:
    Cluster* select_cluster(const core::Task& task) override;
};

} // namespace schedsim::algo
