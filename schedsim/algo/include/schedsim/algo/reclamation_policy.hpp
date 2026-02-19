#pragma once

#include <schedsim/core/types.hpp>

namespace schedsim::algo {

class CbsServer;

/// @brief Abstract interface for bandwidth reclamation policies.
/// @ingroup algo_policies
///
/// Reclamation policies allow unused CBS bandwidth to be redistributed
/// among active servers. Concrete implementations (GrubPolicy, CashPolicy)
/// define how surplus budget is detected, tracked, and reused.
///
/// The policy hooks into the CBS lifecycle at three points:
/// - Early job completion (budget left over)
/// - Budget exhaustion (server needs more time)
/// - Virtual time computation (scaling the virtual clock)
///
/// @see GrubPolicy
/// @see CashPolicy
/// @see CbsServer
class ReclamationPolicy {
public:
    virtual ~ReclamationPolicy() = default;

    /// @brief Handle early job completion (job finishes before budget exhaustion).
    ///
    /// Called when a job completes with remaining budget. The policy decides
    /// whether the server should enter a NonContending state (GRUB) or
    /// follow standard CBS behavior (Inactive/Ready).
    ///
    /// @param server           The CBS server whose job completed early.
    /// @param remaining_budget The unused budget at the time of completion.
    /// @return true if the server should enter NonContending state (GRUB behaviour);
    ///         false for standard CBS behaviour (server goes Inactive/Ready).
    /// @see GrubPolicy::on_early_completion
    /// @see CashPolicy::on_early_completion
    virtual bool on_early_completion(CbsServer& server, core::Duration remaining_budget) = 0;

    /// @brief Handle budget exhaustion for a running server.
    ///
    /// Called when a server's budget reaches zero while its job is still
    /// executing. The policy may grant extra budget (e.g. CASH borrowing
    /// from the spare queue) or return zero for standard CBS postponement.
    ///
    /// @param server The CBS server whose budget is exhausted.
    /// @return Extra budget granted to the server; zero means standard CBS
    ///         deadline postponement applies.
    /// @see GrubPolicy::on_budget_exhausted
    /// @see CashPolicy::on_budget_exhausted
    virtual core::Duration on_budget_exhausted(CbsServer& server) = 0;

    /// @brief Compute the next virtual time after consuming execution time.
    ///
    /// Default implementation uses the standard CBS formula:
    ///   vt += exec_time / U_server.
    /// GRUB overrides this with vt += exec_time / U_active (clamped to avoid
    /// division by zero).
    ///
    /// @param server     The CBS server whose virtual time is being updated.
    /// @param current_vt The server's current virtual time.
    /// @param exec_time  The wall-clock execution time consumed since the
    ///                    last virtual time update.
    /// @return The updated virtual time.
    /// @see GrubPolicy::compute_virtual_time
    virtual core::TimePoint compute_virtual_time(
        const CbsServer& server, core::TimePoint current_vt, core::Duration exec_time) const;

    /// @brief Server state transitions observed by the reclamation policy.
    ///
    /// Notified via on_server_state_change() so the policy can track which
    /// servers are active and adjust bandwidth accounting accordingly.
    enum class ServerStateChange {
        Activated,       ///< Inactive -> Ready (first job arrival)
        Dispatched,      ///< Ready -> Running (server begins executing)
        Preempted,       ///< Running -> Ready (higher-priority preemption)
        Completed,       ///< Running -> Inactive (job finished, no pending jobs)
        NonContending,   ///< Running -> NonContending (GRUB early completion)
        DeadlineReached, ///< NonContending -> Inactive (GRUB deadline expiry)
        Detached         ///< Server removed from scheduler (no future arrivals)
    };

    /// @brief Notify the policy of a server state transition.
    ///
    /// Called whenever a CBS server changes state so the policy can update
    /// its internal bandwidth tracking (e.g. active utilization sums).
    ///
    /// @param server The CBS server whose state changed.
    /// @param change The type of state transition that occurred.
    virtual void on_server_state_change(CbsServer& server, ServerStateChange change) = 0;

    /// @brief Compute the dynamic budget for a running server.
    ///
    /// Default implementation returns the server's static remaining budget.
    /// M-GRUB overrides this to scale the budget by the bandwidth factor.
    ///
    /// @param server The CBS server whose budget is being queried.
    /// @return The effective budget duration for timer scheduling.
    /// @see GrubPolicy::compute_server_budget
    virtual core::Duration compute_server_budget(const CbsServer& server) const;

    /// @brief Whether the scheduler must recalculate budget timers for all
    ///        running servers after each reschedule.
    ///
    /// Default is false (standard CBS). M-GRUB returns true because budget
    /// consumption rates change when the active utilization changes.
    ///
    /// @return true if global budget recalculation is required.
    virtual bool needs_global_budget_recalculation() const { return false; }

    /// @brief Current bandwidth factor used for virtual time rate computation.
    ///
    /// Default is 1.0 (no reclamation effect). GRUB overrides this with the
    /// M-GRUB formula that accounts for multiprocessor bandwidth sharing.
    ///
    /// @return The bandwidth scaling factor in [kMinUtilization, 1.0].
    /// @see GrubPolicy::compute_bandwidth
    [[nodiscard]] virtual double compute_bandwidth() const { return 1.0; }

    /// @brief Get the current active utilization.
    ///
    /// Active utilization is the sum of U_i for all servers currently in the
    /// Running or Ready state. Used by DVFS policies to set processor
    /// frequencies proportional to actual workload.
    ///
    /// @return The sum of utilizations of active (Ready/Running) servers.
    [[nodiscard]] virtual double active_utilization() const = 0;

    /// @brief Get the "in-scheduler" utilization.
    ///
    /// Sum of U_i for servers that have been activated at least once and have
    /// not yet been detached. Used by the PA (Piloted Approach) DVFS policy.
    /// Default implementation delegates to active_utilization().
    ///
    /// @return The sum of utilizations of in-scheduler servers.
    /// @see GrubPolicy::scheduler_utilization
    [[nodiscard]] virtual double scheduler_utilization() const { return active_utilization(); }

    /// @brief Get the maximum utilization among in-scheduler servers.
    ///
    /// Returns the largest U_i among servers that have been activated at least
    /// once and not yet detached. Used by PA DVFS for the u_max term.
    /// Default returns 0.0; callers should fall back to max_server_utilization().
    ///
    /// @return The maximum single-server utilization, or 0.0 if untracked.
    /// @see GrubPolicy::max_scheduler_utilization
    [[nodiscard]] virtual double max_scheduler_utilization() const { return 0.0; }
};

} // namespace schedsim::algo
