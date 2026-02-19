#pragma once

/// @file ffa_policy.hpp
/// @brief FFA (Feedback-based Frequency Adaptation) DVFS+DPM policy.
/// @ingroup algo_policies

#include <schedsim/algo/dvfs_dpm_utils.hpp>
#include <schedsim/algo/dvfs_policy.hpp>

#include <schedsim/core/types.hpp>

#include <unordered_map>

namespace schedsim::core {
class ClockDomain;
class Engine;
class Processor;
} // namespace schedsim::core

namespace schedsim::algo {

class EdfScheduler;

/// @brief FFA (Feedback-based Frequency Adaptation) DVFS+DPM policy.
///
/// Computes a minimum frequency using the PA formula first, then checks
/// whether the resulting frequency falls below the domain's efficient
/// frequency (freq_eff). When it does, the policy reduces the number of
/// active processors instead of lowering frequency further, putting excess
/// idle cores to sleep.
///
/// Frequency-first: lower frequency as much as possible, then reduce cores.
///
/// @ingroup algo_policies
/// @see DvfsPolicy, CsfPolicy, FfaTimerPolicy, dvfs_dpm::compute_freq_min
class FfaPolicy : public DvfsPolicy {
public:
    /// @brief Construct an FFA policy.
    ///
    /// @param engine         The simulation engine (for time queries and tracing).
    /// @param dvfs_cooldown  Minimum interval between consecutive frequency changes.
    /// @param sleep_cstate   C-state level to use when putting idle processors to sleep.
    FfaPolicy(core::Engine& engine,
              core::Duration dvfs_cooldown = core::duration_from_seconds(0.0),
              int sleep_cstate = 1);

    /// @brief Recompute target frequency and active core count after a utilization change.
    ///
    /// @param scheduler The EDF scheduler providing utilization data.
    /// @param domain    The clock domain to adjust.
    void on_utilization_changed(EdfScheduler& scheduler, core::ClockDomain& domain) override;

    /// @brief Handle a processor becoming idle (potential DPM sleep candidate).
    ///
    /// @param scheduler The EDF scheduler owning the processor.
    /// @param proc      The processor that became idle.
    void on_processor_idle(EdfScheduler& scheduler, core::Processor& proc) override;

    /// @brief Handle a processor becoming active (woken from sleep or assigned work).
    ///
    /// @param scheduler The EDF scheduler owning the processor.
    /// @param proc      The processor that became active.
    void on_processor_active(EdfScheduler& scheduler, core::Processor& proc) override;

    /// @brief Return the configured cooldown period.
    ///
    /// @return The DVFS cooldown duration passed at construction.
    [[nodiscard]] core::Duration cooldown_period() const override;

protected:
    /// @brief Compute the target frequency and active processor count.
    ///
    /// Can be overridden by subclasses (e.g., FfaTimerPolicy) to customize
    /// the computation while reusing the apply logic.
    ///
    /// @param active_util  Aggregate active utilization in the domain.
    /// @param max_util     Maximum single-server utilization in the domain.
    /// @param total_procs  Total number of processors in the domain.
    /// @param domain       The clock domain being adjusted.
    /// @return A PlatformTarget with the desired frequency and active processor count.
    [[nodiscard]] virtual dvfs_dpm::PlatformTarget compute_target(
        double active_util, double max_util,
        std::size_t total_procs, const core::ClockDomain& domain) const;

    core::Engine& engine_;
    core::Duration dvfs_cooldown_;
    int sleep_cstate_;
    std::unordered_map<std::size_t, CooldownTimer> domain_cooldowns_;
};

} // namespace schedsim::algo
