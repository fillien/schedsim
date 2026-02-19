#pragma once

#include <schedsim/algo/reclamation_policy.hpp>

#include <schedsim/core/timer.hpp>
#include <schedsim/core/types.hpp>

#include <set>
#include <unordered_map>
#include <unordered_set>

namespace schedsim::algo {

class CbsServer;
class EdfScheduler;

/// @brief M-GRUB (Multiprocessor Greedy Reclamation of Unused Bandwidth) policy.
/// @ingroup algo_policies
///
/// Implements the GRUB bandwidth reclamation algorithm extended for
/// multiprocessor systems (M-GRUB). The policy maintains two utilization
/// aggregates:
///
/// - **Active utilization** (Ready + Running servers) -- used by DVFS
///   policies to scale processor frequency proportionally to real demand.
/// - **In-scheduler utilization** (all non-detached servers) -- used in the
///   M-GRUB bandwidth formula that governs virtual time progression.
///
/// When a job completes early, the server enters a NonContending state and
/// a deadline timer is armed. The server's bandwidth is released from the
/// active set immediately, allowing other servers to consume time at a
/// faster virtual rate. When the deadline timer fires, the server returns
/// to Inactive and its bandwidth is also released from the in-scheduler set.
///
/// @see ReclamationPolicy
/// @see CashPolicy
/// @see CbsServer
/// @see EdfScheduler
class GrubPolicy : public ReclamationPolicy {
public:
    /// @brief Minimum utilization floor to prevent division by zero in the
    ///        GRUB virtual time formula.
    static constexpr double kMinUtilization = 0.01;

    /// @brief Construct a GrubPolicy attached to the given EDF scheduler.
    /// @param scheduler The EDF scheduler that owns the CBS servers managed
    ///                  by this policy.
    explicit GrubPolicy(EdfScheduler& scheduler);

    /// @brief Destructor; cancels any outstanding deadline timers.
    ~GrubPolicy() override;

    /// @brief Handle early job completion by entering NonContending state.
    ///
    /// The server transitions to NonContending and a deadline timer is armed
    /// so that the server's bandwidth is reclaimed when its current deadline
    /// expires.
    ///
    /// @param server           The CBS server whose job completed early.
    /// @param remaining_budget The unused budget at the time of completion.
    /// @return Always true (GRUB always enters NonContending on early completion).
    /// @see ReclamationPolicy::on_early_completion
    bool on_early_completion(CbsServer& server, core::Duration remaining_budget) override;

    /// @brief Handle budget exhaustion with standard CBS postponement.
    ///
    /// GRUB does not grant extra budget; it relies on virtual time scaling
    /// instead. Returns zero so that the scheduler performs a normal CBS
    /// deadline postponement.
    ///
    /// @param server The CBS server whose budget is exhausted.
    /// @return Always zero (no extra budget granted).
    /// @see ReclamationPolicy::on_budget_exhausted
    core::Duration on_budget_exhausted(CbsServer& server) override;

    /// @brief Compute virtual time using the GRUB reclamation formula.
    ///
    /// Advances virtual time by exec_time scaled by the inverse of the
    /// M-GRUB bandwidth factor, so that servers consume their virtual
    /// budget faster when overall active utilization is low.
    ///
    /// @param server     The CBS server whose virtual time is being updated.
    /// @param current_vt The server's current virtual time.
    /// @param exec_time  Wall-clock execution time consumed since last update.
    /// @return The updated virtual time.
    /// @see ReclamationPolicy::compute_virtual_time
    /// @see compute_bandwidth
    core::TimePoint compute_virtual_time(
        const CbsServer& server, core::TimePoint current_vt, core::Duration exec_time) const override;

    /// @brief Update utilization tracking on server state transitions.
    ///
    /// Adjusts both active and in-scheduler utilization sums, and manages
    /// deadline timers for NonContending servers.
    ///
    /// @param server The CBS server whose state changed.
    /// @param change The type of state transition that occurred.
    /// @see ReclamationPolicy::on_server_state_change
    void on_server_state_change(CbsServer& server, ServerStateChange change) override;

    /// @brief Get the current active utilization (Ready + Running servers).
    /// @return Sum of U_i for active servers.
    [[nodiscard]] double active_utilization() const override { return active_utilization_; }

    /// @brief Get the in-scheduler utilization (all non-detached servers).
    /// @return Sum of U_i for servers activated at least once and not detached.
    /// @see ReclamationPolicy::scheduler_utilization
    [[nodiscard]] double scheduler_utilization() const override { return scheduler_utilization_; }

    /// @brief Get the maximum utilization ever seen among in-scheduler servers.
    ///
    /// This value is monotonically non-decreasing: it is never reduced when
    /// a server is detached. Used by PA DVFS for the u_max term.
    ///
    /// @return The historical maximum single-server utilization.
    /// @see ReclamationPolicy::max_scheduler_utilization
    [[nodiscard]] double max_scheduler_utilization() const override {
        return max_ever_scheduler_util_;
    }

    /// @brief Compute the M-GRUB dynamic budget for a running server.
    ///
    /// Scales the server's remaining budget by the inverse of the bandwidth
    /// factor, yielding the wall-clock time until budget exhaustion given
    /// the current active utilization.
    ///
    /// @param server The CBS server whose budget is being queried.
    /// @return The scaled budget duration for timer scheduling.
    /// @see ReclamationPolicy::compute_server_budget
    /// @see compute_bandwidth
    core::Duration compute_server_budget(const CbsServer& server) const override;

    /// @brief M-GRUB requires global budget recalculation after each reschedule.
    /// @return Always true.
    /// @see ReclamationPolicy::needs_global_budget_recalculation
    bool needs_global_budget_recalculation() const override { return true; }

    /// @brief Compute the M-GRUB bandwidth factor.
    ///
    /// The bandwidth factor is max(kMinUtilization,
    /// min(1.0, m - (m-1) * U_max / U_sched)) where m is the number of
    /// processors and U_max / U_sched are derived from the in-scheduler set.
    ///
    /// @return The bandwidth scaling factor in [kMinUtilization, 1.0].
    /// @see ReclamationPolicy::compute_bandwidth
    [[nodiscard]] double compute_bandwidth() const override;

private:
    /// @brief Arm a deadline timer for a NonContending server.
    /// @param server The server entering NonContending state.
    void schedule_deadline_timer(CbsServer& server);

    /// @brief Cancel an outstanding deadline timer for a server.
    /// @param server The server whose timer should be cancelled.
    void cancel_deadline_timer(CbsServer& server);

    /// @brief Callback invoked when a NonContending server's deadline expires.
    /// @param server The server whose deadline has been reached.
    void on_deadline_reached(CbsServer& server);

    EdfScheduler& scheduler_; ///< The EDF scheduler owning the managed servers.

    /// @brief Sum of U_i for servers in Ready or Running state (DVFS input).
    double active_utilization_{0.0};

    /// @brief Sum of U_i for all non-detached servers (M-GRUB bandwidth formula input).
    double scheduler_utilization_{0.0};

    /// @brief Sorted multiset of per-server utilizations in the in-scheduler set.
    ///
    /// Maintained to allow efficient lookup of the maximum utilization for the
    /// M-GRUB bandwidth formula.
    std::multiset<double> scheduler_utils_;

    /// @brief Set of server pointers currently in the in-scheduler set.
    std::unordered_set<const CbsServer*> in_scheduler_set_;

    /// @brief Historical maximum utilization of any server ever in the scheduler.
    ///
    /// Monotonically non-decreasing; never reduced on detach. Used by PA DVFS.
    double max_ever_scheduler_util_{0.0};

    /// @brief Map from NonContending servers to their deadline timer IDs.
    std::unordered_map<CbsServer*, core::TimerId> deadline_timers_;
};

} // namespace schedsim::algo
