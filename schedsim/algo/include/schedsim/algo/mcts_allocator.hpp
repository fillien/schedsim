#pragma once

#include <schedsim/algo/multi_cluster_allocator.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace schedsim::algo {

/// @brief Monte Carlo Tree Search (MCTS) allocator.
/// @ingroup algo_allocators
///
/// Replays a pre-computed allocation pattern produced by an offline MCTS
/// solver.  Each entry in the pattern vector is a cluster index that
/// determines where the corresponding task (in arrival order) is placed.
/// When the pattern is exhausted, the allocator falls back to a
/// pseudo-random selection using a splitmix64-style RNG.
///
/// @see MultiClusterAllocator for the base class and task-binding semantics.
/// @see FFBigFirstAllocator for a simpler heuristic alternative.
class MCTSAllocator : public MultiClusterAllocator {
public:
    /// @brief Construct an MCTS allocator with a pre-computed cluster pattern.
    /// @param engine  Reference to the simulation engine.
    /// @param clusters  Vector of pointers to the available clusters.
    /// @param pattern  Sequence of cluster indices (one per expected task arrival).
    MCTSAllocator(core::Engine& engine, std::vector<Cluster*> clusters,
                  std::vector<unsigned> pattern);

    /// @brief Return the number of allocation steps performed so far.
    /// @return The cumulative count of select_cluster invocations.
    [[nodiscard]] std::size_t allocation_count() const noexcept { return step_; }

protected:
    /// @brief Select a cluster according to the pre-computed pattern or RNG fallback.
    /// @param task The task to be allocated.
    /// @return Pointer to the chosen Cluster.  Never returns @c nullptr since the
    ///         RNG fallback always produces a valid cluster index.
    /// @see MultiClusterAllocator::select_cluster
    Cluster* select_cluster(const core::Task& task) override;

private:
    /// @brief Pre-computed sequence of cluster indices from offline MCTS.
    std::vector<unsigned> pattern_;
    /// @brief Running count of allocation steps (also indexes into pattern_).
    std::size_t step_{0};
    /// @brief Internal state for the splitmix64 pseudo-random fallback.
    std::uint64_t rng_state_{0x9E3779B97F4A7C15ULL};
};

} // namespace schedsim::algo
