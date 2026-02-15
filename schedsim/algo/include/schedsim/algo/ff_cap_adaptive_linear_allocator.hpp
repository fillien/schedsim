#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

namespace schedsim::algo {

class FFCapAdaptiveLinearAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

    void set_expected_total_util(double util) noexcept { expected_total_util_ = util; }

protected:
    Cluster* select_cluster(const core::Task& task) override;

private:
    double observed_umax_{0.0};
    double expected_total_util_{0.0};
};

} // namespace schedsim::algo
