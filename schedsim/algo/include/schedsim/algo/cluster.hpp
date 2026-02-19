#pragma once

#include <schedsim/core/types.hpp>

#include <cstddef>
#include <optional>

namespace schedsim::core {
class ClockDomain;
} // namespace schedsim::core

namespace schedsim::algo {

class Scheduler;

/// @brief Lightweight algo-layer wrapper tying a ClockDomain to a Scheduler.
/// @ingroup algo_allocators
///
/// A Cluster binds a hardware ClockDomain to a scheduling entity and carries
/// allocation metadata such as performance score, frequency-normalisation
/// factor, and utilization target. It lives in the algo layer (rather than
/// core) because it couples scheduling concepts (Scheduler, admission,
/// u_target) to hardware concepts (ClockDomain).
///
/// Used by partitioned allocators to represent one scheduling unit (one or
/// more processors sharing a clock domain and a scheduler).
///
/// @see Allocator, Scheduler, core::ClockDomain
class Cluster {
public:
    /// @brief Construct a Cluster binding a clock domain to a scheduler.
    ///
    /// @param clock_domain    The hardware clock domain for this cluster.
    /// @param scheduler       The scheduler managing processors in this cluster.
    /// @param perf_score      Performance score of the cluster's processor type
    ///                        (dimensionless; higher is faster).
    /// @param reference_freq_max Maximum frequency of the reference cluster,
    ///                           used for cross-cluster normalisation.
    Cluster(core::ClockDomain& clock_domain, Scheduler& scheduler,
            double perf_score, double reference_freq_max);

    /// @brief Get the underlying clock domain (mutable).
    /// @return Reference to the ClockDomain.
    [[nodiscard]] core::ClockDomain& clock_domain() noexcept { return clock_domain_; }

    /// @brief Get the underlying clock domain (const).
    /// @return Const reference to the ClockDomain.
    [[nodiscard]] const core::ClockDomain& clock_domain() const noexcept { return clock_domain_; }

    /// @brief Get the associated scheduler (mutable).
    /// @return Reference to the Scheduler.
    [[nodiscard]] Scheduler& scheduler() noexcept { return scheduler_; }

    /// @brief Get the associated scheduler (const).
    /// @return Const reference to the Scheduler.
    [[nodiscard]] const Scheduler& scheduler() const noexcept { return scheduler_; }

    /// @brief Get the performance score of this cluster's processor type.
    /// @return Dimensionless performance score (higher is faster).
    [[nodiscard]] double perf() const noexcept { return perf_score_; }

    /// @brief Compute the cross-cluster frequency normalisation factor.
    ///
    /// Returns reference_freq_max / this_cluster_freq_max, matching the
    /// legacy Cluster::scale_speed() semantics.
    ///
    /// @return Frequency scaling ratio (>=1.0 for slower clusters).
    [[nodiscard]] double scale_speed() const noexcept;

    /// @brief Get the utilization target for allocation decisions.
    /// @return Current utilization target in [0, 1].
    [[nodiscard]] double u_target() const noexcept { return u_target_; }

    /// @brief Set the utilization target for allocation decisions.
    ///
    /// Mutable because adaptive allocators may adjust it at run time.
    ///
    /// @param target New utilization target in [0, 1].
    void set_u_target(double target) noexcept { u_target_ = target; }

    /// @brief Scale a task's utilization to this cluster.
    ///
    /// Computes task_util * scale_speed() / perf(), converting a
    /// reference-normalised utilization into the cluster-local value.
    ///
    /// @param task_util The task utilization in reference units.
    /// @return Scaled utilization for this cluster.
    [[nodiscard]] double scaled_utilization(double task_util) const noexcept;

    /// @brief Set the processor ID when this cluster wraps a single processor.
    ///
    /// Used in per-core partitioned mode where each cluster contains
    /// exactly one processor.
    ///
    /// @param id The processor ID.
    void set_processor_id(std::size_t id) noexcept;

    /// @brief Get the single-processor ID, if set.
    /// @return The processor ID, or std::nullopt if not in per-core mode.
    [[nodiscard]] std::optional<std::size_t> processor_id() const noexcept;

    /// @brief Get the remaining scheduling capacity.
    ///
    /// Computed as processor_count - raw utilization. Uses raw (reference)
    /// utilization rather than scaled utilization. Worst-fit / best-fit
    /// allocators rank clusters by this value within the
    /// scaled-utilization-admissible set.
    ///
    /// @return Remaining capacity as a dimensionless value.
    [[nodiscard]] double remaining_capacity() const noexcept;

    // -------------------------------------------------------------------
    // Delegated queries
    // -------------------------------------------------------------------

    /// @brief Get the number of processors in this cluster.
    /// @return Processor count (delegated to Scheduler).
    /// @see Scheduler::processor_count
    [[nodiscard]] std::size_t processor_count() const noexcept;

    /// @brief Get the total utilization of the cluster's scheduler.
    /// @return Total server utilization (delegated to Scheduler).
    /// @see Scheduler::utilization
    [[nodiscard]] double utilization() const noexcept;

    /// @brief Check if a new server can be admitted to this cluster.
    /// @param budget The server budget.
    /// @param period The server period.
    /// @return True if the server can be admitted.
    /// @see Scheduler::can_admit
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
