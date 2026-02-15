#pragma once

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

// FFA (Fixed Frequency with Allocation) DVFS+DPM policy
// Computes minimum frequency and active processor count to meet utilization.
// When frequency drops below freq_eff, reduces active cores instead.
class FfaPolicy : public DvfsPolicy {
public:
    FfaPolicy(core::Engine& engine,
              core::Duration dvfs_cooldown = core::Duration{0.0},
              int sleep_cstate = 1);

    void on_utilization_changed(EdfScheduler& scheduler, core::ClockDomain& domain) override;
    void on_processor_idle(EdfScheduler& scheduler, core::Processor& proc) override;
    void on_processor_active(EdfScheduler& scheduler, core::Processor& proc) override;
    [[nodiscard]] core::Duration cooldown_period() const override;

protected:
    [[nodiscard]] virtual dvfs_dpm::PlatformTarget compute_target(
        double active_util, double max_util,
        std::size_t total_procs, const core::ClockDomain& domain) const;

    core::Engine& engine_;
    core::Duration dvfs_cooldown_;
    int sleep_cstate_;
    std::unordered_map<std::size_t, CooldownTimer> domain_cooldowns_;
};

} // namespace schedsim::algo
