#pragma once

#include <schedsim/core/types.hpp>

namespace schedsim::algo {

class CbsServer;

// Interface for bandwidth reclamation policies
// Reclamation policies allow unused bandwidth to be redistributed to other servers
class ReclamationPolicy {
public:
    virtual ~ReclamationPolicy() = default;

    // Called on early job completion (job finishes before budget exhaustion)
    // Returns true if server should enter NonContending state (GRUB)
    // Returns false for standard CBS behavior (server goes Inactive/Ready)
    virtual bool on_early_completion(CbsServer& server, core::Duration remaining_budget) = 0;

    // Called on budget exhaustion
    // Returns extra budget granted (0 = standard CBS postpone)
    // CASH can return borrowed budget from spare queue
    virtual core::Duration on_budget_exhausted(CbsServer& server) = 0;

    // Virtual time computation (override for GRUB formula)
    // Default: CBS formula vt += exec_time / U_server
    // GRUB: vt += exec_time / U_active (clamped to avoid division by zero)
    virtual core::TimePoint compute_virtual_time(
        const CbsServer& server, core::TimePoint current_vt, core::Duration exec_time) const;

    // State change notification for tracking active bandwidth
    // Called when a server transitions between states
    enum class ServerStateChange {
        Activated,       // Inactive -> Ready
        Dispatched,      // Ready -> Running
        Preempted,       // Running -> Ready
        Completed,       // Running -> Inactive
        NonContending,   // Running -> NonContending (GRUB)
        DeadlineReached, // NonContending -> Inactive (GRUB)
        Detached         // Server removed from scheduler (Inactive, no future arrivals)
    };
    virtual void on_server_state_change(CbsServer& server, ServerStateChange change) = 0;

    // Compute dynamic budget for a running server.
    // Default: CBS static budget (server.remaining_budget())
    virtual core::Duration compute_server_budget(const CbsServer& server) const;

    // Whether the scheduler should recalculate budget timers for ALL running servers
    // after each reschedule. Default: false.
    virtual bool needs_global_budget_recalculation() const { return false; }

    // Current bandwidth used for virtual time rate computation.
    // Default: 1.0 (no reclamation effect). GRUB overrides with M-GRUB formula.
    [[nodiscard]] virtual double compute_bandwidth() const { return 1.0; }

    // Get current active utilization (for DVFS integration)
    // Active utilization = sum of U_i for servers in Running or Ready state
    [[nodiscard]] virtual double active_utilization() const = 0;

    // Get "in-scheduler" utilization: sum of U_i for servers that have been
    // activated at least once and not yet detached. Used by PA DVFS.
    // Default: same as active_utilization (no distinction without M-GRUB).
    [[nodiscard]] virtual double scheduler_utilization() const { return active_utilization(); }

    // Get max utilization among in-scheduler servers. Used by PA DVFS.
    // Default: 0.0 (caller should fall back to max_server_utilization).
    [[nodiscard]] virtual double max_scheduler_utilization() const { return 0.0; }
};

} // namespace schedsim::algo
