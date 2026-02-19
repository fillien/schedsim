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

/// @brief Earliest Deadline First scheduler with CBS bandwidth servers.
///
/// Manages a set of processors and dispatches jobs to CBS servers
/// ordered by absolute deadline. Supports GRUB/CASH reclamation,
/// DVFS frequency scaling policies (PA, FFA, CSF), and DPM power
/// management.
///
/// Each task is assigned a CBS server via add_server(). When a job
/// arrives, the scheduler replenishes the server budget (if needed),
/// inserts the server into the EDF ready queue, and dispatches the
/// highest-priority server to an available processor.
///
/// @par Example
/// @code
/// core::Engine engine;
/// auto& plat = engine.platform();
/// auto& pt = plat.add_processor_type("cpu", 1.0);
/// auto& cd = plat.add_clock_domain(Frequency{500.0}, Frequency{2000.0});
/// auto& pd = plat.add_power_domain(
///     {{0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}});
/// auto& proc = plat.add_processor(pt, cd, pd);
/// auto& task = plat.add_task(
///     duration_from_seconds(0.01),   // period: 10 ms
///     duration_from_seconds(0.01),   // deadline: 10 ms
///     duration_from_seconds(0.005)); // WCET: 5 ms
/// engine.finalize();
///
/// algo::EdfScheduler sched(engine, {&proc});
/// sched.add_server(task);
/// sched.enable_grub();
/// engine.run();
/// @endcode
///
/// @see CbsServer, Scheduler, GrubPolicy, CashPolicy
/// @ingroup algo_schedulers
class EdfScheduler : public Scheduler {
public:
    /// @brief Construct an EDF scheduler managing the given processors.
    /// @param engine Reference to the simulation engine.
    /// @param processors Processors that this scheduler dispatches to.
    EdfScheduler(core::Engine& engine, std::vector<core::Processor*> processors);
    ~EdfScheduler() override;

    EdfScheduler(const EdfScheduler&) = delete;
    EdfScheduler& operator=(const EdfScheduler&) = delete;
    EdfScheduler(EdfScheduler&&) = delete;
    EdfScheduler& operator=(EdfScheduler&&) = delete;

    /// @name Scheduler Interface
    /// @{

    /// @brief Handle a new job arrival for a task.
    /// @param task The task that owns the arriving job.
    /// @param job The new job instance.
    void on_job_arrival(core::Task& task, core::Job job) override;

    /// @brief Set the expected number of job arrivals for a task.
    ///
    /// Used by M-GRUB for server detach: when all expected arrivals
    /// have been received and the server becomes Inactive, the server
    /// is detached from the scheduler to reduce active utilization.
    ///
    /// @param task The task to track.
    /// @param count Total number of expected job arrivals.
    void set_expected_arrivals(const core::Task& task, std::size_t count) override;

    /// @brief Test whether a new server with the given utilization can be admitted.
    /// @param budget Requested server budget.
    /// @param period Requested server period.
    /// @return True if admitting the server would not exceed capacity.
    [[nodiscard]] bool can_admit(core::Duration budget, core::Duration period) const override;

    /// @brief Returns the total utilization of all servers.
    /// @return Total utilization as a dimensionless ratio.
    [[nodiscard]] double utilization() const override;

    /// @}

    /// @brief Returns a view of the managed processors.
    /// @return Read-only span of processor pointers.
    [[nodiscard]] std::span<core::Processor* const> processors() const;

    /// @brief Returns the number of managed processors.
    /// @return Processor count.
    [[nodiscard]] std::size_t processor_count() const noexcept override { return processors_.size(); }

    /// @brief Attempt to detach a server from the scheduler (M-GRUB).
    ///
    /// A server is detached when it is Inactive and all expected job
    /// arrivals have been received. Detached servers no longer
    /// contribute to active utilization.
    ///
    /// @param server The CBS server to try detaching.
    void try_detach_server(CbsServer& server);

    /// @name Server Management
    /// @{

    /// @brief Add a CBS server for a task with explicit budget and period.
    ///
    /// The server utilization (budget/period) is checked against the
    /// admission test. Throws AdmissionError if over capacity.
    ///
    /// @param task Task to associate with the server.
    /// @param budget Server budget per period.
    /// @param period Server replenishment period.
    /// @param policy What to do when the budget is exhausted mid-job.
    /// @return Reference to the created CbsServer.
    /// @throws AdmissionError if admission test fails.
    CbsServer& add_server(core::Task& task, core::Duration budget, core::Duration period,
                          CbsServer::OverrunPolicy policy = CbsServer::OverrunPolicy::Queue);

    /// @brief Add a CBS server using the task's WCET as budget and the task's period.
    /// @param task Task to associate with the server.
    /// @return Reference to the created CbsServer.
    CbsServer& add_server(core::Task& task);

    /// @brief Add a CBS server without running the admission test.
    ///
    /// Use for testing or advanced scenarios where the caller manages capacity.
    ///
    /// @param task Task to associate with the server.
    /// @param budget Server budget per period.
    /// @param period Server replenishment period.
    /// @param policy What to do when the budget is exhausted mid-job.
    /// @return Reference to the created CbsServer.
    CbsServer& add_server_unchecked(core::Task& task, core::Duration budget, core::Duration period,
                                     CbsServer::OverrunPolicy policy = CbsServer::OverrunPolicy::Queue);

    /// @brief Find the CBS server for a task, or nullptr if none.
    /// @param task The task to look up.
    /// @return Pointer to the CBS server, or nullptr.
    [[nodiscard]] CbsServer* find_server(core::Task& task);
    /// @brief Find the CBS server for a task (const), or nullptr if none.
    /// @param task The task to look up.
    /// @return Const pointer to the CBS server, or nullptr.
    [[nodiscard]] const CbsServer* find_server(const core::Task& task) const;

    /// @brief Returns the number of CBS servers.
    /// @return Server count.
    [[nodiscard]] std::size_t server_count() const noexcept { return servers_.size(); }

    /// @}

    /// @name Configuration
    /// @{

    /// @brief Set the admission test mode.
    /// @param test The admission test to use.
    void set_admission_test(AdmissionTest test);

    /// @brief Set the policy for handling deadline misses on processors.
    /// @param policy The deadline miss policy to apply.
    void set_deadline_miss_policy(DeadlineMissPolicy policy);

    /// @brief Set a callback invoked when a running job misses its deadline.
    /// @param handler Callback receiving the processor and job.
    void set_deadline_miss_handler(std::function<void(core::Processor&, core::Job&)> handler);

    /// @brief Set a callback invoked when a queued job misses its deadline.
    /// @param handler Callback receiving the job.
    void set_queued_deadline_miss_handler(std::function<void(core::Job&)> handler);

    /// @}

    /// @name Policy Management
    /// @{

    /// @brief Set the bandwidth reclamation policy (GRUB or CASH).
    /// @param policy Reclamation policy (ownership transferred).
    void set_reclamation_policy(std::unique_ptr<ReclamationPolicy> policy);

    /// @brief Set the DVFS frequency scaling policy.
    /// @param policy DVFS policy (ownership transferred).
    void set_dvfs_policy(std::unique_ptr<DvfsPolicy> policy);

    /// @brief Set the DPM power management policy.
    /// @param policy DPM policy (ownership transferred).
    void set_dpm_policy(std::unique_ptr<DpmPolicy> policy);

    /// @}

    /// @name Convenience Policy Methods
    /// @brief Shorthand for creating and setting standard policies.
    /// @{

    /// @brief Enable GRUB bandwidth reclamation.
    void enable_grub();

    /// @brief Enable CASH bandwidth reclamation.
    void enable_cash();

    /// @brief Enable Power-Aware DVFS.
    /// @param cooldown Minimum time between frequency changes.
    void enable_power_aware_dvfs(core::Duration cooldown = core::duration_from_seconds(0.0));

    /// @brief Enable basic DPM (put idle cores to sleep).
    /// @param target_cstate Target C-state for idle cores.
    void enable_basic_dpm(int target_cstate = 1);

    /// @brief Enable FFA (Feedback-based Frequency Adaptation) with integrated DPM.
    /// @param cooldown Minimum time between frequency changes.
    /// @param sleep_cstate Target C-state for idle cores.
    void enable_ffa(core::Duration cooldown = core::duration_from_seconds(0.0), int sleep_cstate = 1);

    /// @brief Enable CSF (Cluster-level Sleep Frequency) with integrated DPM.
    /// @param cooldown Minimum time between frequency changes.
    /// @param sleep_cstate Target C-state for idle cores.
    void enable_csf(core::Duration cooldown = core::duration_from_seconds(0.0), int sleep_cstate = 1);

    /// @brief Enable timer-based FFA variant.
    /// @param cooldown Minimum time between frequency changes.
    /// @param sleep_cstate Target C-state for idle cores.
    void enable_ffa_timer(core::Duration cooldown = core::duration_from_seconds(0.0), int sleep_cstate = 1);

    /// @brief Enable timer-based CSF variant.
    /// @param cooldown Minimum time between frequency changes.
    /// @param sleep_cstate Target C-state for idle cores.
    void enable_csf_timer(core::Duration cooldown = core::duration_from_seconds(0.0), int sleep_cstate = 1);

    /// @}

    /// @name Utilization Queries
    /// @{

    /// @brief Returns the sum of utilization for currently active servers.
    /// @return Active utilization as a dimensionless ratio.
    [[nodiscard]] double active_utilization() const;

    /// @brief Returns the sum of utilization for servers in the scheduler (activated, not detached).
    /// @return Scheduler utilization as a dimensionless ratio.
    [[nodiscard]] double scheduler_utilization() const;

    /// @brief Returns the maximum utilization among in-scheduler servers.
    /// @return Maximum single-server utilization in the scheduler.
    [[nodiscard]] double max_scheduler_utilization() const;

    /// @brief Returns the maximum utilization among all servers.
    /// @return Maximum single-server utilization.
    [[nodiscard]] double max_server_utilization() const;

    /// @}

    /// @brief Access the simulation engine.
    /// @return Reference to the engine.
    [[nodiscard]] core::Engine& engine() noexcept { return engine_; }
    /// @brief Access the simulation engine (const).
    /// @return Const reference to the engine.
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
    void recalculate_all_budget_timers();
    void flush_running_server_times();

    // Queued deadline timers (for jobs waiting in CBS server queue, not on a processor)
    void schedule_queued_deadline_timer(CbsServer& server);
    void cancel_queued_deadline_timer(CbsServer& server);
    void on_queued_deadline_miss(CbsServer& server);

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

    // Admission test helper
    [[nodiscard]] double admission_capacity(double new_util) const;

    core::Engine& engine_;
    std::vector<core::Processor*> processors_;
    std::deque<CbsServer> servers_;  // deque to prevent pointer invalidation on growth

    std::unordered_map<const core::Task*, CbsServer*> task_to_server_;
    std::unordered_map<CbsServer*, core::Processor*> server_to_processor_;
    std::unordered_map<core::Processor*, CbsServer*> processor_to_server_;
    std::unordered_map<CbsServer*, core::TimerId> budget_timers_;
    std::unordered_map<CbsServer*, core::TimerId> queued_deadline_timers_;

    core::DeferredId resched_deferred_;
    double total_utilization_{0.0};
    double reference_performance_;

    AdmissionTest admission_test_{AdmissionTest::CapacityBound};
    DeadlineMissPolicy deadline_miss_policy_{DeadlineMissPolicy::Continue};
    std::function<void(core::Processor&, core::Job&)> deadline_miss_handler_;
    std::function<void(core::Job&)> queued_deadline_miss_handler_;

    // Track last dispatch time per server for budget consumption calculation
    std::unordered_map<CbsServer*, core::TimePoint> last_dispatch_time_;

    // Monotonic server ID counter for deterministic tie-breaking
    std::size_t next_server_id_{0};

    // Policies (Phase 6)
    std::unique_ptr<ReclamationPolicy> reclamation_policy_;
    std::unique_ptr<DvfsPolicy> dvfs_policy_;
    std::unique_ptr<DpmPolicy> dpm_policy_;

    // Arrival tracking for server detach (M-GRUB)
    std::unordered_map<const core::Task*, std::size_t> expected_arrivals_;
    std::unordered_map<const core::Task*, std::size_t> arrival_counts_;
};

} // namespace schedsim::algo
