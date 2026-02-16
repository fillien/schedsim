#include <schedsim/algo/grub_policy.hpp>

#include <schedsim/algo/cbs_server.hpp>
#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/event.hpp>
#include <schedsim/core/task.hpp>

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
    // M-GRUB NonContending condition: vt > now && vt < deadline
    // (matches legacy scheduler.cpp:303-304)
    auto now = scheduler_.engine().time();
    auto vt = server.virtual_time();
    auto dl = server.deadline();

    if (vt > now && vt < dl) {
        // Schedule timer at VT (not deadline) — matches legacy server.cpp:91
        schedule_deadline_timer(server);
        return true;  // Server should enter NonContending
    }

    return false;
}

core::Duration GrubPolicy::on_budget_exhausted(CbsServer& /*server*/) {
    // GRUB doesn't grant extra budget on exhaustion
    return core::Duration{0.0};
}

core::TimePoint GrubPolicy::compute_virtual_time(
    const CbsServer& server, core::TimePoint current_vt, core::Duration exec_time) const {
    // M-GRUB per-server formula (matches legacy parallel.cpp:59-68):
    // vt += (bandwidth / U_i) * exec_time
    double bandwidth = compute_bandwidth();
    double vt_increment = (bandwidth / server.utilization()) * exec_time.count();
    return core::TimePoint{current_vt.time_since_epoch() + core::Duration{vt_increment}};
}

double GrubPolicy::compute_bandwidth() const {
    // M-GRUB bandwidth formula (matches legacy parallel.cpp:30-57 and verify_grub.py:246-263)
    if (scheduler_utils_.empty()) {
        return 1.0;
    }

    auto m = static_cast<double>(scheduler_.processor_count());
    double u_max = *scheduler_utils_.rbegin();
    double total_u = scheduler_utilization_;
    double inactive_bw = m - (m - 1.0) * u_max - total_u;
    double bandwidth = std::max(1.0 - inactive_bw / m, kMinUtilization);
    return bandwidth;
}

core::Duration GrubPolicy::compute_server_budget(const CbsServer& server) const {
    // M-GRUB dynamic budget (matches legacy parallel.cpp:41-51):
    // budget = (U_i / bandwidth) * (deadline - vt)
    double bandwidth = compute_bandwidth();
    double dt = (server.deadline() - server.virtual_time()).count();
    if (dt <= 0.0) {
        return core::Duration{0.0};
    }
    double budget = (server.utilization() / bandwidth) * dt;
    return core::Duration{std::max(budget, 0.0)};
}

void GrubPolicy::on_server_state_change(CbsServer& server, ServerStateChange change) {
    double util = server.utilization();

    switch (change) {
        case ServerStateChange::Activated:
            // Inactive -> Ready: add to active set
            active_utilization_ += util;
            // Add to scheduler set if not already there
            if (in_scheduler_set_.find(&server) == in_scheduler_set_.end()) {
                scheduler_utilization_ += util;
                scheduler_utils_.insert(util);
                in_scheduler_set_.insert(&server);
                max_ever_scheduler_util_ = std::max(max_ever_scheduler_util_, util);
            }
            // Cancel any pending deadline timer (reactivation from NonContending)
            cancel_deadline_timer(server);
            break;

        case ServerStateChange::Dispatched:
            // Ready -> Running: no changes to either set
            break;

        case ServerStateChange::Preempted:
            // Running -> Ready: no changes to either set
            break;

        case ServerStateChange::Completed:
            // Running -> Inactive: remove from active set (stays in scheduler set)
            active_utilization_ -= util;
            break;

        case ServerStateChange::NonContending:
            // Running -> NonContending: remove from active set (stays in scheduler set)
            active_utilization_ -= util;
            break;

        case ServerStateChange::DeadlineReached:
            // NonContending -> Inactive: no changes (already removed from active at NonContending)
            break;

        case ServerStateChange::Detached:
            // Server removed from scheduler: remove from scheduler set
            if (in_scheduler_set_.find(&server) != in_scheduler_set_.end()) {
                scheduler_utilization_ -= util;
                auto it = scheduler_utils_.find(util);
                if (it != scheduler_utils_.end()) {
                    scheduler_utils_.erase(it);
                }
                in_scheduler_set_.erase(&server);
            }
            break;
    }

    // Ensure we don't go negative due to floating point errors
    if (active_utilization_ < 0.0) {
        active_utilization_ = 0.0;
    }
    if (scheduler_utilization_ < 0.0) {
        scheduler_utilization_ = 0.0;
    }
}

void GrubPolicy::schedule_deadline_timer(CbsServer& server) {
    // Cancel any existing timer
    cancel_deadline_timer(server);

    // M-GRUB: schedule timer at VT (not deadline) — matches legacy server.cpp:91
    core::TimePoint target = server.virtual_time();

    // Only schedule if target is in the future
    if (target > scheduler_.engine().time()) {
        deadline_timers_[&server] = scheduler_.engine().add_timer(
            target,
            core::EventPriority::TIMER_DEFAULT,
            [this, &server]() { on_deadline_reached(server); });
    } else {
        // Target already passed, transition immediately
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

    // Emit serv_inactive
    scheduler_.engine().trace([&](core::TraceWriter& w) {
        w.type("serv_inactive");
        w.field("tid", static_cast<uint64_t>(server.task()->id()));
    });

    // Notify state change (update utilization tracking)
    on_server_state_change(server, ServerStateChange::DeadlineReached);

    // Check if server should be detached from scheduler
    scheduler_.try_detach_server(server);
}

} // namespace schedsim::algo
