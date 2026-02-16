#include <schedsim/algo/ffa_policy.hpp>

#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>

#include <algorithm>
#include <cmath>

namespace schedsim::algo {

FfaPolicy::FfaPolicy(core::Engine& engine, core::Duration dvfs_cooldown, int sleep_cstate)
    : engine_(engine)
    , dvfs_cooldown_(dvfs_cooldown)
    , sleep_cstate_(sleep_cstate) {}

void FfaPolicy::on_utilization_changed(EdfScheduler& scheduler, core::ClockDomain& domain) {
    if (domain.is_locked() || domain.is_transitioning()) {
        return;
    }

    // Check cooldown
    auto [it, inserted] = domain_cooldowns_.try_emplace(
        domain.id(), engine_, dvfs_cooldown_);
    if (!it->second.can_change()) {
        return;
    }

    double scale = dvfs_dpm::compute_utilization_scale(
        scheduler.engine().platform(), domain);
    double active_util = scheduler.active_utilization() * scale;
    double max_util = scheduler.max_scheduler_utilization() * scale;
    auto total_procs = domain.processors().size();

    auto target = compute_target(active_util, max_util, total_procs, domain);

    dvfs_dpm::apply_platform_target(scheduler, domain, target, sleep_cstate_, on_frequency_changed_);

    it->second.start_cooldown();
}

void FfaPolicy::on_processor_idle(EdfScheduler& /*scheduler*/, core::Processor& /*proc*/) {
    // No-op: FFA manages DPM in on_utilization_changed
}

void FfaPolicy::on_processor_active(EdfScheduler& /*scheduler*/, core::Processor& /*proc*/) {
    // No-op: FFA manages DPM in on_utilization_changed
}

core::Duration FfaPolicy::cooldown_period() const {
    return dvfs_cooldown_;
}

dvfs_dpm::PlatformTarget FfaPolicy::compute_target(
    double active_util, double max_util,
    std::size_t total_procs, const core::ClockDomain& domain) const {

    double freq_max = domain.freq_max().mhz;
    double freq_eff = domain.freq_eff().mhz;
    auto procs_d = static_cast<double>(total_procs);

    double freq_min = dvfs_dpm::compute_freq_min(freq_max, active_util, max_util, procs_d);
    freq_min = std::min(freq_min, freq_max);

    core::Frequency target_freq;
    std::size_t active_procs;

    if (freq_eff > 0.0 && freq_min < freq_eff) {
        // Below efficient frequency: keep freq_eff, reduce cores
        target_freq = domain.ceil_to_mode(core::Frequency{freq_eff});
        double needed = procs_d * freq_min / freq_eff;
        active_procs = dvfs_dpm::clamp_procs(std::ceil(needed), total_procs);
    } else {
        // Above efficient frequency: use all cores, raise frequency
        target_freq = domain.ceil_to_mode(core::Frequency{freq_min});
        active_procs = total_procs;
    }

    return {target_freq, active_procs};
}

} // namespace schedsim::algo
