#include <schedsim/algo/cash_policy.hpp>

#include <schedsim/algo/cbs_server.hpp>
#include <schedsim/algo/edf_scheduler.hpp>

namespace schedsim::algo {

CashPolicy::CashPolicy(EdfScheduler& scheduler)
    : scheduler_(scheduler) {}

bool CashPolicy::on_early_completion(CbsServer& /*server*/, core::Duration remaining_budget) {
    // CASH: Add remaining budget to spare queue for redistribution
    spare_budget_ += remaining_budget;

    // CASH doesn't use NonContending state - server goes directly to Inactive/Ready
    return false;
}

core::Duration CashPolicy::on_budget_exhausted(CbsServer& /*server*/) {
    // CASH: Try to borrow from spare queue
    if (spare_budget_.count() > 0.0) {
        // Grant all available spare budget
        core::Duration granted = spare_budget_;
        spare_budget_ = core::Duration{0.0};
        return granted;
    }

    // No spare budget available, standard CBS postpone will occur
    return core::Duration{0.0};
}

void CashPolicy::on_server_state_change(CbsServer& server, ServerStateChange change) {
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
            // CASH doesn't use NonContending, but handle it for completeness
            active_utilization_ -= util;
            break;

        case ServerStateChange::DeadlineReached:
            // CASH doesn't use NonContending, but handle it for completeness
            break;

        case ServerStateChange::Detached:
            // CASH doesn't use detach, but handle for completeness
            break;
    }

    // Ensure we don't go negative due to floating point errors
    if (active_utilization_ < 0.0) {
        active_utilization_ = 0.0;
    }
}

} // namespace schedsim::algo
