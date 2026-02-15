#pragma once

#include <schedsim/algo/cbs_server.hpp>
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/reclamation_policy.hpp>
#include <schedsim/algo/scheduler.hpp>

#include <schedsim/core/deferred.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/timer.hpp>
#include <schedsim/core/types.hpp>

#include <deque>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace schedsim::algo {

// Forward declarations for policy types
class DpmPolicy;
class DvfsPolicy;
class ReclamationPolicy;

// EDF (Earliest Deadline First) scheduler with CBS servers
// Manages a set of processors and dispatches jobs using EDF ordering
class EdfScheduler : public Scheduler {
public:
    // Construct with engine and processors to manage
    EdfScheduler(core::Engine& engine, std::vector<core::Processor*> processors);
    ~EdfScheduler() override;

    // Non-copyable, non-movable (has callbacks registered with engine)
    EdfScheduler(const EdfScheduler&) = delete;
    EdfScheduler& operator=(const EdfScheduler&) = delete;
    EdfScheduler(EdfScheduler&&) = delete;
    EdfScheduler& operator=(EdfScheduler&&) = delete;

    // Scheduler interface
    void on_job_arrival(core::Task& task, core::Job job) override;
    [[nodiscard]] bool can_admit(core::Duration budget, core::Duration period) const override;
    [[nodiscard]] double utilization() const override;

    // EdfScheduler-specific (not in abstract interface)
    [[nodiscard]] std::span<core::Processor* const> processors() const;
    [[nodiscard]] std::size_t processor_count() const noexcept { return processors_.size(); }

    // Server management
    // Add a server with explicit budget and period (throws AdmissionError if over capacity)
    CbsServer& add_server(core::Task& task, core::Duration budget, core::Duration period,
                          CbsServer::OverrunPolicy policy = CbsServer::OverrunPolicy::Queue);

    // Add a server with task's WCET as budget and period
    CbsServer& add_server(core::Task& task);

    // Add a server without admission test (for testing or advanced use)
    CbsServer& add_server_unchecked(core::Task& task, core::Duration budget, core::Duration period,
                                     CbsServer::OverrunPolicy policy = CbsServer::OverrunPolicy::Queue);

    // Server lookup
    [[nodiscard]] CbsServer* find_server(core::Task& task);
    [[nodiscard]] const CbsServer* find_server(const core::Task& task) const;
    [[nodiscard]] std::size_t server_count() const noexcept { return servers_.size(); }

    // Configuration
    void set_deadline_miss_policy(DeadlineMissPolicy policy);
    void set_deadline_miss_handler(std::function<void(core::Processor&, core::Job&)> handler);

    // Policy management (Phase 6)
    void set_reclamation_policy(std::unique_ptr<ReclamationPolicy> policy);
    void set_dvfs_policy(std::unique_ptr<DvfsPolicy> policy);
    void set_dpm_policy(std::unique_ptr<DpmPolicy> policy);

    // Convenience methods for enabling policies
    void enable_grub();
    void enable_cash();
    void enable_power_aware_dvfs(core::Duration cooldown = core::Duration{0.0});
    void enable_basic_dpm(int target_cstate = 1);
    void enable_ffa(core::Duration cooldown = core::Duration{0.0}, int sleep_cstate = 1);
    void enable_csf(core::Duration cooldown = core::Duration{0.0}, int sleep_cstate = 1);
    void enable_ffa_timer(core::Duration cooldown = core::Duration{0.0}, int sleep_cstate = 1);
    void enable_csf_timer(core::Duration cooldown = core::Duration{0.0}, int sleep_cstate = 1);

    // Active utilization query (for DVFS integration)
    [[nodiscard]] double active_utilization() const;

    // Maximum individual server utilization (for FFA/CSF u_max)
    [[nodiscard]] double max_server_utilization() const;

    // Access to engine (for tests)
    [[nodiscard]] core::Engine& engine() noexcept { return engine_; }
    [[nodiscard]] const core::Engine& engine() const noexcept { return engine_; }

private:
    // ISR handlers (installed on all processors in constructor)
    void on_job_completion(core::Processor& proc, core::Job& job);
    void on_deadline_miss(core::Processor& proc, core::Job& job);
    void on_processor_available(core::Processor& proc);

    // Deferred callback (registered in constructor)
    void on_resched();
    void request_resched();

    // EDF dispatch
    void dispatch_edf();
    void assign_server_to_processor(CbsServer& server, core::Processor& proc);
    void preempt_processor(core::Processor& proc);

    // Budget timers
    void schedule_budget_timer(CbsServer& server, core::Processor& proc);
    void cancel_budget_timer(CbsServer& server);
    void on_budget_exhausted(CbsServer& server);

    // Helper to find server for a processor
    CbsServer* find_server_on_processor(core::Processor& proc);

    // Internal helpers
    std::vector<CbsServer*> get_ready_servers();
    std::vector<core::Processor*> get_available_processors();

    // Policy callbacks
    void notify_utilization_changed();
    void notify_server_state_change(CbsServer& server, ReclamationPolicy::ServerStateChange change);
    void on_dvfs_frequency_changed(core::ClockDomain& domain);
    void reschedule_budget_timers_for_domain(core::ClockDomain& domain);

    core::Engine& engine_;
    std::vector<core::Processor*> processors_;
    std::deque<CbsServer> servers_;  // deque to prevent pointer invalidation on growth

    std::unordered_map<const core::Task*, CbsServer*> task_to_server_;
    std::unordered_map<CbsServer*, core::Processor*> server_to_processor_;
    std::unordered_map<core::Processor*, CbsServer*> processor_to_server_;
    std::unordered_map<CbsServer*, core::TimerId> budget_timers_;

    core::DeferredId resched_deferred_;
    double total_utilization_{0.0};
    double reference_performance_;

    DeadlineMissPolicy deadline_miss_policy_{DeadlineMissPolicy::Continue};
    std::function<void(core::Processor&, core::Job&)> deadline_miss_handler_;

    // Track last dispatch time per server for budget consumption calculation
    std::unordered_map<CbsServer*, core::TimePoint> last_dispatch_time_;

    // Monotonic server ID counter for deterministic tie-breaking
    std::size_t next_server_id_{0};

    // Policies (Phase 6)
    std::unique_ptr<ReclamationPolicy> reclamation_policy_;
    std::unique_ptr<DvfsPolicy> dvfs_policy_;
    std::unique_ptr<DpmPolicy> dpm_policy_;
};

} // namespace schedsim::algo
