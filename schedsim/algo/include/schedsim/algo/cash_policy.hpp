#pragma once

#include <schedsim/algo/reclamation_policy.hpp>

#include <schedsim/core/types.hpp>

namespace schedsim::algo {

class CbsServer;
class EdfScheduler;

// CASH (Capacity Sharing) policy
// Accumulates unused budget from early completions into a spare queue
// Servers can borrow from the spare queue when their budget is exhausted
class CashPolicy : public ReclamationPolicy {
public:
    explicit CashPolicy(EdfScheduler& scheduler);

    // ReclamationPolicy interface
    bool on_early_completion(CbsServer& server, core::Duration remaining_budget) override;
    core::Duration on_budget_exhausted(CbsServer& server) override;
    // Uses default CBS virtual time formula (inherited from ReclamationPolicy)
    void on_server_state_change(CbsServer& server, ServerStateChange change) override;
    [[nodiscard]] double active_utilization() const override { return active_utilization_; }

    // CASH-specific accessors
    [[nodiscard]] core::Duration spare_budget() const noexcept { return spare_budget_; }

private:
    EdfScheduler& scheduler_;
    core::Duration spare_budget_{0.0};
    double active_utilization_{0.0};
};

} // namespace schedsim::algo
