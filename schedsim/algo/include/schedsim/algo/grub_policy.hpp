#pragma once

#include <schedsim/algo/reclamation_policy.hpp>

#include <schedsim/core/timer.hpp>
#include <schedsim/core/types.hpp>

#include <unordered_map>

namespace schedsim::algo {

class CbsServer;
class EdfScheduler;

// GRUB (Greedy Reclamation of Unused Bandwidth) policy
// Tracks active utilization and uses NonContending state for efficient bandwidth reclamation
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

private:
    void schedule_deadline_timer(CbsServer& server);
    void cancel_deadline_timer(CbsServer& server);
    void on_deadline_reached(CbsServer& server);

    EdfScheduler& scheduler_;
    double active_utilization_{0.0};
    std::unordered_map<CbsServer*, core::TimerId> deadline_timers_;
};

} // namespace schedsim::algo
