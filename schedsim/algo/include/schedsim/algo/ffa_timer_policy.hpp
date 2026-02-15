#pragma once

#include <schedsim/algo/dvfs_dpm_utils.hpp>
#include <schedsim/algo/ffa_policy.hpp>

#include <schedsim/core/timer.hpp>

#include <cstddef>
#include <unordered_map>

namespace schedsim::algo {

// Timer-deferred FFA policy.
// Defers DVFS+DPM application by dvfs_cooldown using engine timers.
// Zero cooldown delegates to parent (immediate mode).
class FfaTimerPolicy : public FfaPolicy {
public:
    using FfaPolicy::FfaPolicy;  // Same constructor

    ~FfaTimerPolicy() override;

    void on_utilization_changed(EdfScheduler& scheduler, core::ClockDomain& domain) override;

private:
    struct PendingTarget {
        dvfs_dpm::PlatformTarget target;
        core::TimerId timer_id;
        EdfScheduler* scheduler;
        core::ClockDomain* domain;
    };
    std::unordered_map<std::size_t, PendingTarget> pending_;
};

} // namespace schedsim::algo
