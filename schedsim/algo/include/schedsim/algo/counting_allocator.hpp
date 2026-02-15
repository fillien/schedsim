#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

#include <cstddef>

namespace schedsim::algo {

class CountingAllocator : public MultiClusterAllocator {
public:
    using MultiClusterAllocator::MultiClusterAllocator;

    [[nodiscard]] std::size_t allocation_count() const noexcept { return count_; }

protected:
    Cluster* select_cluster(const core::Task& task) override;

private:
    std::size_t count_{0};
};

} // namespace schedsim::algo
