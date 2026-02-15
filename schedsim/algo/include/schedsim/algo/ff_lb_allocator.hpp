#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

class FFLbAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

protected:
    Cluster* select_cluster(const core::Task& task) override;
};

} // namespace schedsim::algo
