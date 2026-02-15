#include <schedsim/algo/mcts_allocator.hpp>

namespace schedsim::algo {

MCTSAllocator::MCTSAllocator(core::Engine& engine, std::vector<Cluster*> clusters,
                             std::vector<unsigned> pattern)
    : MultiClusterAllocator(engine, std::move(clusters))
    , pattern_(std::move(pattern)) {}

Cluster* MCTSAllocator::select_cluster(const core::Task& /*task*/) {
    auto n = clusters().size();
    if (n == 0) {
        return nullptr;
    }

    Cluster* selected = nullptr;
    if (step_ < pattern_.size()) {
        unsigned idx = pattern_[step_];
        if (idx < n) {
            selected = clusters()[idx];
        }
    } else {
        // XOR-shift PRNG fallback (matches legacy pick_random())
        rng_state_ ^= rng_state_ >> 12;
        rng_state_ ^= rng_state_ << 25;
        rng_state_ ^= rng_state_ >> 27;
        auto rng_val = rng_state_ * 2685821657736338717ULL;
        selected = clusters()[rng_val % n];
    }

    ++step_;
    return selected;
}

} // namespace schedsim::algo
