#include <schedsim/algo/grub_policy.hpp>

#include <schedsim/algo/cbs_server.hpp>
#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/event.hpp>

#include <algorithm>
#include <cassert>

namespace schedsim::algo {

GrubPolicy::GrubPolicy(EdfScheduler& scheduler)
    : scheduler_(scheduler) {}

GrubPolicy::~GrubPolicy() {
    // Cancel any pending deadline timers
    for (auto& [server, timer] : deadline_timers_) {
        scheduler_.engine().cancel_timer(timer);
    }
}

bool GrubPolicy::on_early_completion(CbsServer& server, core::Duration /*remaining_budget*/) {
    // GRUB: server should enter NonContending state and wait for its deadline
    // Schedule a timer for when the deadline is reached
    schedule_deadline_timer(server);
    return true;  // Server should enter NonContending
}

core::Duration GrubPolicy::on_budget_exhausted(CbsServer& /*server*/) {
    // GRUB doesn't grant extra budget on exhaustion
    // The standard CBS postpone mechanism is used
    return core::Duration{0.0};
}

core::TimePoint GrubPolicy::compute_virtual_time(
    const CbsServer& /*server*/, core::TimePoint current_vt, core::Duration exec_time) const {
    // GRUB formula: vt += exec_time / max(active_util, min_util)
    // Clamp to minimum to avoid division by zero when no servers are active
    double effective_util = std::max(active_utilization_, kMinUtilization);
    core::Duration vt_increment{exec_time.count() / effective_util};
    return core::TimePoint{current_vt.time_since_epoch() + vt_increment};
}

void GrubPolicy::on_server_state_change(CbsServer& server, ServerStateChange change) {
    double util = server.utilization();

    switch (change) {
        case ServerStateChange::Activated:
            // Inactive -> Ready: add to active utilization
            active_utilization_ += util;
            break;

        case ServerStateChange::Dispatched:
            // Ready -> Running: already counted in active utilization
            break;

        case ServerStateChange::Preempted:
            // Running -> Ready: still counted in active utilization
            break;

        case ServerStateChange::Completed:
            // Running -> Inactive: remove from active utilization
            active_utilization_ -= util;
            break;

        case ServerStateChange::NonContending:
            // Running -> NonContending: remove from active utilization
            // Server is waiting for deadline but not contending for CPU
            active_utilization_ -= util;
            break;

        case ServerStateChange::DeadlineReached:
            // NonContending -> Inactive: already removed from active utilization
            break;
    }

    // Ensure we don't go negative due to floating point errors
    if (active_utilization_ < 0.0) {
        active_utilization_ = 0.0;
    }
}

void GrubPolicy::schedule_deadline_timer(CbsServer& server) {
    // Cancel any existing timer
    cancel_deadline_timer(server);

    // Schedule timer for server's deadline
    core::TimePoint deadline = server.deadline();

    // Only schedule if deadline is in the future
    if (deadline > scheduler_.engine().time()) {
        deadline_timers_[&server] = scheduler_.engine().add_timer(
            deadline,
            core::EventPriority::TIMER_DEFAULT,
            [this, &server]() { on_deadline_reached(server); });
    } else {
        // Deadline already passed, transition immediately
        on_deadline_reached(server);
    }
}

void GrubPolicy::cancel_deadline_timer(CbsServer& server) {
    auto it = deadline_timers_.find(&server);
    if (it != deadline_timers_.end()) {
        scheduler_.engine().cancel_timer(it->second);
        deadline_timers_.erase(it);
    }
}

void GrubPolicy::on_deadline_reached(CbsServer& server) {
    // Remove timer from map (it has fired)
    deadline_timers_.erase(&server);

    // Only process if server is still in NonContending state
    if (server.state() != CbsServer::State::NonContending) {
        return;
    }

    // Transition to Inactive
    server.reach_deadline(scheduler_.engine().time());

    // Notify state change (for consistency, though active_util already updated)
    on_server_state_change(server, ServerStateChange::DeadlineReached);
}

} // namespace schedsim::algo
