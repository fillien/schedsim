#pragma once

#include <schedsim/algo/reclamation_policy.hpp>

#include <schedsim/core/types.hpp>

namespace schedsim::algo {

class CbsServer;
class EdfScheduler;

/// @brief CASH (Capacity Sharing) bandwidth reclamation policy.
/// @ingroup algo_policies
///
/// Implements the CASH algorithm, an alternative to GRUB for redistributing
/// unused CBS bandwidth. When a job completes early, its remaining budget is
/// deposited into a global spare queue rather than entering a NonContending
/// state. Servers whose budget is exhausted can then borrow from the spare
/// queue to continue executing without a deadline postponement.
///
/// CASH uses the standard CBS virtual time formula (inherited from
/// ReclamationPolicy) and does not modify virtual time scaling.
///
/// @see ReclamationPolicy
/// @see GrubPolicy
/// @see CbsServer
/// @see EdfScheduler
class CashPolicy : public ReclamationPolicy {
public:
    /// @brief Construct a CashPolicy attached to the given EDF scheduler.
    /// @param scheduler The EDF scheduler that owns the CBS servers managed
    ///                  by this policy.
    explicit CashPolicy(EdfScheduler& scheduler);

    /// @brief Handle early job completion by depositing unused budget.
    ///
    /// Adds the remaining budget to the global spare queue so that other
    /// servers may borrow it later.
    ///
    /// @param server           The CBS server whose job completed early.
    /// @param remaining_budget The unused budget to deposit into the spare queue.
    /// @return Always false (CASH does not use the NonContending state).
    /// @see ReclamationPolicy::on_early_completion
    bool on_early_completion(CbsServer& server, core::Duration remaining_budget) override;

    /// @brief Handle budget exhaustion by borrowing from the spare queue.
    ///
    /// If the spare queue has accumulated budget from early completions,
    /// the server receives some or all of it as extra execution time.
    ///
    /// @param server The CBS server whose budget is exhausted.
    /// @return The amount of extra budget granted from the spare queue;
    ///         zero if the spare queue is empty (standard CBS postponement).
    /// @see ReclamationPolicy::on_budget_exhausted
    core::Duration on_budget_exhausted(CbsServer& server) override;

    /// @brief Update active utilization tracking on server state transitions.
    ///
    /// CASH tracks active utilization (Ready + Running) for DVFS integration
    /// but does not maintain an in-scheduler set (no NonContending state).
    ///
    /// @param server The CBS server whose state changed.
    /// @param change The type of state transition that occurred.
    /// @see ReclamationPolicy::on_server_state_change
    void on_server_state_change(CbsServer& server, ServerStateChange change) override;

    /// @brief Get the current active utilization (Ready + Running servers).
    /// @return Sum of U_i for active servers.
    [[nodiscard]] double active_utilization() const override { return active_utilization_; }

    /// @brief Get the total spare budget currently available for borrowing.
    /// @return The accumulated unused budget in the spare queue.
    [[nodiscard]] core::Duration spare_budget() const noexcept { return spare_budget_; }

private:
    EdfScheduler& scheduler_;        ///< The EDF scheduler owning the managed servers.
    core::Duration spare_budget_{};  ///< Accumulated unused budget from early completions.
    double active_utilization_{0.0}; ///< Sum of U_i for Ready/Running servers.
};

} // namespace schedsim::algo
