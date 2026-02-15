#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace schedsim::algo {

class MCTSAllocator : public MultiClusterAllocator {
public:
    MCTSAllocator(core::Engine& engine, std::vector<Cluster*> clusters,
                  std::vector<unsigned> pattern);

    [[nodiscard]] std::size_t allocation_count() const noexcept { return step_; }

protected:
    Cluster* select_cluster(const core::Task& task) override;

private:
    std::vector<unsigned> pattern_;
    std::size_t step_{0};
    std::uint64_t rng_state_{0x9E3779B97F4A7C15ULL};
};

} // namespace schedsim::algo
