#pragma once

#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/types.hpp>

#include <cstdint>
#include <deque>

namespace schedsim::algo {

class EdfScheduler;

// CBS (Constant Bandwidth Server) implementation
// Manages a queue of jobs for a single task with bandwidth reservation
class CbsServer {
public:
    // Server state machine states
    enum class State {
        Inactive,      // No pending jobs
        Ready,         // Has pending jobs, waiting for dispatch
        Running,       // Currently executing on a processor
        NonContending  // GRUB: job completed early, waiting for deadline (Phase 6)
    };

    // Policy for handling job overruns (new arrival while job is running)
    enum class OverrunPolicy {
        Queue,  // Queue the new job (default)
        Skip,   // Drop the new job
        Abort   // Abort current job, start new one
    };

    CbsServer(std::size_t id, core::Duration budget, core::Duration period,
              OverrunPolicy policy = OverrunPolicy::Queue);

    // Server ID accessor (for deterministic tie-breaking in EDF)
    [[nodiscard]] std::size_t id() const noexcept { return id_; }

    // Non-copyable (has internal state, pointers)
    CbsServer(const CbsServer&) = delete;
    CbsServer& operator=(const CbsServer&) = delete;

    // Movable (for vector storage)
    CbsServer(CbsServer&&) = default;
    CbsServer& operator=(CbsServer&&) = default;

    ~CbsServer() = default;

    // CBS parameters
    [[nodiscard]] core::Duration budget() const noexcept { return budget_; }
    [[nodiscard]] core::Duration period() const noexcept { return period_; }
    [[nodiscard]] double utilization() const noexcept { return utilization_; }

    // State accessors
    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] core::TimePoint deadline() const noexcept { return deadline_; }
    [[nodiscard]] core::TimePoint virtual_time() const noexcept { return virtual_time_; }
    [[nodiscard]] core::Duration remaining_budget() const noexcept { return remaining_budget_; }
    [[nodiscard]] OverrunPolicy overrun_policy() const noexcept { return overrun_policy_; }

    // Job queue accessors
    [[nodiscard]] bool has_pending_jobs() const noexcept { return !job_queue_.empty(); }
    [[nodiscard]] std::size_t job_queue_size() const noexcept { return job_queue_.size(); }
    [[nodiscard]] uint64_t last_enqueued_job_id() const noexcept { return last_enqueued_job_id_; }
    [[nodiscard]] core::Job* current_job();
    [[nodiscard]] const core::Job* current_job() const;

    // Job queue management
    void enqueue_job(core::Job job);
    core::Job dequeue_job();

    // State transitions
    // Inactive -> Ready: job arrives, initialize deadline and virtual time
    void activate(core::TimePoint current_time);

    // Ready -> Running: EDF selects this server
    void dispatch();

    // Running -> Ready: higher priority server preempts
    void preempt();

    // Running -> Ready (has jobs) or Inactive (no jobs): job completes
    void complete_job(core::TimePoint current_time);

    // Running -> Ready: budget exhausted, postpone deadline
    void exhaust_budget(core::TimePoint current_time);

    // Running -> NonContending: job completed early, waiting for deadline (GRUB)
    void enter_non_contending(core::TimePoint current_time);

    // NonContending -> Inactive: server deadline reached (GRUB)
    void reach_deadline(core::TimePoint current_time);

    // CBS formulas
    // Update virtual time: vt += execution_time / U
    void update_virtual_time(core::Duration execution_time);

    // Postpone deadline: d += T, remaining_budget = Q
    void postpone_deadline();

    // Budget consumption
    void consume_budget(core::Duration amount);

    // Task association (set by EdfScheduler)
    [[nodiscard]] core::Task* task() const noexcept { return task_; }

private:
    friend class EdfScheduler;

    void set_task(core::Task* task) noexcept { task_ = task; }
    void set_scheduler(EdfScheduler* scheduler) noexcept { scheduler_ = scheduler; }

    std::size_t id_;
    core::Duration budget_;
    core::Duration period_;
    double utilization_;
    OverrunPolicy overrun_policy_;

    State state_{State::Inactive};
    core::TimePoint deadline_{core::Duration{0.0}};
    core::TimePoint virtual_time_{core::Duration{0.0}};
    core::Duration remaining_budget_{core::Duration{0.0}};
    uint64_t job_counter_{0};
    uint64_t last_enqueued_job_id_{0};

    std::deque<core::Job> job_queue_;
    core::Task* task_{nullptr};
    EdfScheduler* scheduler_{nullptr};
};

} // namespace schedsim::algo
