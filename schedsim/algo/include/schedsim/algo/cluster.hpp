#pragma once

#include <schedsim/core/types.hpp>

#include <cstddef>
#include <optional>

namespace schedsim::core {
class ClockDomain;
} // namespace schedsim::core

namespace schedsim::algo {

class Scheduler;

// Lightweight algo-layer wrapper tying a ClockDomain to a Scheduler with allocation metadata.
// Lives in algo (not core) because it binds scheduling concepts (Scheduler, admission, u_target)
// to hardware concepts (ClockDomain).
class Cluster {
public:
    Cluster(core::ClockDomain& clock_domain, Scheduler& scheduler,
            double perf_score, double reference_freq_max);

    [[nodiscard]] core::ClockDomain& clock_domain() noexcept { return clock_domain_; }
    [[nodiscard]] const core::ClockDomain& clock_domain() const noexcept { return clock_domain_; }
    [[nodiscard]] Scheduler& scheduler() noexcept { return scheduler_; }
    [[nodiscard]] const Scheduler& scheduler() const noexcept { return scheduler_; }

    // Performance score of this cluster's processor type
    [[nodiscard]] double perf() const noexcept { return perf_score_; }

    // Cross-cluster frequency normalization: ref_cluster.freq_max / this.freq_max
    // Matches legacy Cluster::scale_speed() semantics
    [[nodiscard]] double scale_speed() const noexcept;

    // Utilization target for allocation (mutable, used by adaptive allocators)
    [[nodiscard]] double u_target() const noexcept { return u_target_; }
    void set_u_target(double target) noexcept { u_target_ = target; }

    // Scale a task's utilization to this cluster = task_util * scale_speed() / perf()
    [[nodiscard]] double scaled_utilization(double task_util) const noexcept;

    // Optional: set when this cluster wraps a single processor (per-core mode)
    void set_processor_id(std::size_t id) noexcept;
    [[nodiscard]] std::optional<std::size_t> processor_id() const noexcept;

    // Remaining scheduling capacity: processor_count - raw utilization.
    // Uses raw (reference) utilization, not scaled. WF/BF allocators rank by this
    // value within the scaled_utilization-admissible set; any placement in that set
    // is correct, so raw capacity is a sufficient load-balancing heuristic.
    [[nodiscard]] double remaining_capacity() const noexcept;

    // Delegated queries
    [[nodiscard]] std::size_t processor_count() const noexcept;
    [[nodiscard]] double utilization() const noexcept;
    [[nodiscard]] bool can_admit(core::Duration budget, core::Duration period) const;

private:
    core::ClockDomain& clock_domain_;
    Scheduler& scheduler_;
    double perf_score_;
    double reference_freq_max_;  // cluster[0].freq_max for cross-cluster normalization
    double u_target_{1.0};
    std::optional<std::size_t> processor_id_;
};

} // namespace schedsim::algo
