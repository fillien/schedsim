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

// M-GRUB (Multiprocessor Greedy Reclamation of Unused Bandwidth) policy
// Tracks both "in-scheduler" utilization (for bandwidth formula) and
// "active" utilization (for DVFS), using NonContending state for efficient bandwidth reclamation
class GrubPolicy : public ReclamationPolicy {
public:
    // Minimum utilization to avoid division by zero in GRUB formula
    static constexpr double kMinUtilization = 0.01;

    explicit GrubPolicy(EdfScheduler& scheduler);
    ~GrubPolicy() override;

    // ReclamationPolicy interface
    bool on_early_completion(CbsServer& server, core::Duration remaining_budget) override;
    core::Duration on_budget_exhausted(CbsServer& server) override;
    core::TimePoint compute_virtual_time(
        const CbsServer& server, core::TimePoint current_vt, core::Duration exec_time) const override;
    void on_server_state_change(CbsServer& server, ServerStateChange change) override;
    [[nodiscard]] double active_utilization() const override { return active_utilization_; }
    [[nodiscard]] double scheduler_utilization() const override { return scheduler_utilization_; }
    [[nodiscard]] double max_scheduler_utilization() const override {
        return max_ever_scheduler_util_;
    }

    // M-GRUB overrides
    core::Duration compute_server_budget(const CbsServer& server) const override;
    bool needs_global_budget_recalculation() const override { return true; }

    // M-GRUB bandwidth computation
    [[nodiscard]] double compute_bandwidth() const override;

private:
    void schedule_deadline_timer(CbsServer& server);
    void cancel_deadline_timer(CbsServer& server);
    void on_deadline_reached(CbsServer& server);

    EdfScheduler& scheduler_;

    // "Active" tracking (Ready/Running servers — for DVFS)
    double active_utilization_{0.0};

    // "In scheduler" tracking (all non-detached servers — for bandwidth formula)
    double scheduler_utilization_{0.0};
    std::multiset<double> scheduler_utils_;
    std::unordered_set<const CbsServer*> in_scheduler_set_;

    // Max utilization of any server ever in the scheduler (never decremented)
    double max_ever_scheduler_util_{0.0};

    std::unordered_map<CbsServer*, core::TimerId> deadline_timers_;
};

} // namespace schedsim::algo
