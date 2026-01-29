#include <schedsim/algo/dvfs_policy.hpp>

#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/processor.hpp>

#include <algorithm>

namespace schedsim::algo {

// CooldownTimer implementation

CooldownTimer::CooldownTimer(core::Engine& engine, core::Duration cooldown)
    : engine_(engine)
    , cooldown_period_(cooldown) {}

bool CooldownTimer::can_change() const {
    return engine_.time() >= cooldown_until_;
}

void CooldownTimer::start_cooldown() {
    cooldown_until_ = engine_.time() + cooldown_period_;
}

bool CooldownTimer::in_cooldown() const {
    return engine_.time() < cooldown_until_;
}

// PowerAwareDvfsPolicy implementation

PowerAwareDvfsPolicy::PowerAwareDvfsPolicy(core::Engine& engine, core::Duration cooldown)
    : engine_(engine)
    , cooldown_(cooldown) {}

void PowerAwareDvfsPolicy::on_utilization_changed(EdfScheduler& scheduler,
                                                    core::ClockDomain& domain) {
    // Skip if domain is transitioning
    if (domain.is_transitioning()) {
        return;
    }

    // Skip if domain frequency is locked
    if (domain.is_locked()) {
        return;
    }

    // Check cooldown
    auto& cooldown_timer = get_cooldown_timer(domain);
    if (!cooldown_timer.can_change()) {
        return;
    }

    // Compute target frequency based on active utilization
    double active_util = scheduler.active_utilization();
    core::Frequency target = compute_target_frequency(domain, active_util);

    // Apply change if different from current
    apply_frequency_change(domain, target);
}

void PowerAwareDvfsPolicy::on_processor_idle(EdfScheduler& /*scheduler*/,
                                               core::Processor& /*proc*/) {
    // PowerAware doesn't take specific action on idle
    // Utilization-based scaling handles this via on_utilization_changed
}

void PowerAwareDvfsPolicy::on_processor_active(EdfScheduler& /*scheduler*/,
                                                 core::Processor& /*proc*/) {
    // PowerAware doesn't take specific action on active
    // Utilization-based scaling handles this via on_utilization_changed
}

core::Frequency PowerAwareDvfsPolicy::compute_target_frequency(
    const core::ClockDomain& domain, double active_util) const {
    // Formula: f = f_min + (f_max - f_min) * U_active
    // Clamp utilization to [0, 1] for the formula
    double clamped_util = std::clamp(active_util, 0.0, 1.0);

    double f_min = domain.freq_min().mhz;
    double f_max = domain.freq_max().mhz;

    double target_mhz = f_min + (f_max - f_min) * clamped_util;

    // Clamp to valid range
    target_mhz = std::clamp(target_mhz, f_min, f_max);

    return core::Frequency{target_mhz};
}

void PowerAwareDvfsPolicy::apply_frequency_change(core::ClockDomain& domain,
                                                    core::Frequency target) {
    // Only change if different
    if (domain.frequency() == target) {
        return;
    }

    // Check if domain is transitioning (double check)
    if (domain.is_transitioning()) {
        return;
    }

    // Apply the frequency change
    domain.set_frequency(target);

    // Start cooldown timer
    auto& cooldown_timer = get_cooldown_timer(domain);
    cooldown_timer.start_cooldown();

    // Notify scheduler of frequency change
    if (on_frequency_changed_) {
        on_frequency_changed_(domain);
    }
}

CooldownTimer& PowerAwareDvfsPolicy::get_cooldown_timer(core::ClockDomain& domain) {
    auto it = domain_cooldowns_.find(domain.id());
    if (it == domain_cooldowns_.end()) {
        auto [inserted, _] = domain_cooldowns_.emplace(
            domain.id(), CooldownTimer(engine_, cooldown_));
        return inserted->second;
    }
    return it->second;
}

} // namespace schedsim::algo
