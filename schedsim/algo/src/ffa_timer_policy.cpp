#include <schedsim/algo/ffa_timer_policy.hpp>

#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>

namespace schedsim::algo {

FfaTimerPolicy::~FfaTimerPolicy() {
    for (auto& [domain_id, pending] : pending_) {
        engine_.cancel_timer(pending.timer_id);
    }
}

void FfaTimerPolicy::on_utilization_changed(EdfScheduler& scheduler, core::ClockDomain& domain) {
    if (domain.is_locked() || domain.is_transitioning()) {
        return;
    }

    // Zero cooldown: immediate mode (delegate to parent)
    if (dvfs_cooldown_ <= core::Duration::zero()) {
        FfaPolicy::on_utilization_changed(scheduler, domain);
        return;
    }

    double scale = dvfs_dpm::compute_utilization_scale(
        scheduler.engine().platform(), domain);
    double active_util = scheduler.active_utilization() * scale;
    double max_util = scheduler.max_scheduler_utilization() * scale;
    auto total_procs = domain.processors().size();

    auto target = compute_target(active_util, max_util, total_procs, domain);

    // Check if target matches current state — no timer needed
    bool freq_same = (target.frequency == domain.frequency());
    std::size_t current_active = dvfs_dpm::count_active_processors(domain.processors());
    bool procs_same = (target.active_processors == current_active);

    if (freq_same && procs_same) {
        // Cancel any pending timer for this domain
        auto it = pending_.find(domain.id());
        if (it != pending_.end()) {
            engine_.cancel_timer(it->second.timer_id);
            pending_.erase(it);
        }
        return;
    }

    // Cancel existing pending timer for this domain
    auto it = pending_.find(domain.id());
    if (it != pending_.end()) {
        engine_.cancel_timer(it->second.timer_id);
        pending_.erase(it);
    }

    // Schedule deferred application
    core::TimePoint fire_time = engine_.time() + dvfs_cooldown_;
    std::size_t domain_id = domain.id();

    core::TimerId timer_id = engine_.add_timer(
        fire_time,
        core::EventPriority::TIMER_DEFAULT,
        [this, domain_id]() {
            auto it = pending_.find(domain_id);
            if (it == pending_.end()) {
                return;
            }
            auto pending = it->second;
            pending_.erase(it);

            dvfs_dpm::apply_platform_target(
                *pending.scheduler, *pending.domain,
                pending.target, sleep_cstate_, on_frequency_changed_);
        });

    pending_[domain_id] = PendingTarget{target, timer_id, &scheduler, &domain};
}

} // namespace schedsim::algo
