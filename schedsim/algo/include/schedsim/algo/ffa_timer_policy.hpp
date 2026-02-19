#pragma once

/// @file ffa_timer_policy.hpp
/// @brief Timer-deferred variant of the FFA DVFS+DPM policy.
/// @ingroup algo_policies

#include <schedsim/algo/dvfs_dpm_utils.hpp>
#include <schedsim/algo/ffa_policy.hpp>

#include <schedsim/core/timer.hpp>

#include <cstddef>
#include <unordered_map>

namespace schedsim::algo {

/// @brief Timer-deferred FFA (Feedback-based Frequency Adaptation) policy.
///
/// Extends FfaPolicy by deferring the application of DVFS+DPM changes by the
/// configured cooldown duration using engine timers. When a utilization change
/// occurs during an active cooldown, the pending target is updated in place so
/// that only the most recent target is applied when the timer fires.
///
/// If the cooldown is zero, behaviour degrades to the parent FfaPolicy
/// (immediate mode) with no timer overhead.
///
/// @ingroup algo_policies
/// @see FfaPolicy, CsfTimerPolicy, dvfs_dpm::apply_platform_target
class FfaTimerPolicy : public FfaPolicy {
public:
    /// @brief Inherit FfaPolicy constructors.
    using FfaPolicy::FfaPolicy;

    /// @brief Destructor cancels any pending timers.
    ~FfaTimerPolicy() override;

    /// @brief Recompute and defer the target frequency/core-count change.
    ///
    /// If a timer is already pending for this domain, updates the pending
    /// target without rescheduling. Otherwise, starts a new timer that will
    /// apply the target after the cooldown expires.
    ///
    /// @param scheduler The EDF scheduler providing utilization data.
    /// @param domain    The clock domain to adjust.
    void on_utilization_changed(EdfScheduler& scheduler, core::ClockDomain& domain) override;

private:
    /// @brief State for a pending deferred frequency/DPM change.
    struct PendingTarget {
        dvfs_dpm::PlatformTarget target;  ///< Most recent computed target.
        core::TimerId timer_id;           ///< Engine timer handle.
        EdfScheduler* scheduler;          ///< Back-pointer to the scheduler.
        core::ClockDomain* domain;        ///< Back-pointer to the clock domain.
    };

    /// @brief Map from clock-domain ID to pending deferred target.
    std::unordered_map<std::size_t, PendingTarget> pending_;
};

} // namespace schedsim::algo
