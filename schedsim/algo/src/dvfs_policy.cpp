#include <schedsim/algo/dvfs_policy.hpp>

#include <schedsim/algo/dvfs_dpm_utils.hpp>
#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
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

    // Scale utilization for heterogeneous platforms
    double scale = dvfs_dpm::compute_utilization_scale(
        scheduler.engine().platform(), domain);
    double total_util = scheduler.scheduler_utilization() * scale;
    double max_util = scheduler.max_scheduler_utilization() * scale;
    auto nb_procs = static_cast<double>(domain.processors().size());

    // f_min = f_max * ((m-1)*U_max + U_total) / m
    double freq_min = dvfs_dpm::compute_freq_min(
        domain.freq_max().mhz, total_util, max_util, nb_procs);
    core::Frequency target = domain.ceil_to_mode(
        core::Frequency{std::min(freq_min, domain.freq_max().mhz)});

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
