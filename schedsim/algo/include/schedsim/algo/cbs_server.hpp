#pragma once

#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/types.hpp>

#include <cstdint>
#include <deque>

namespace schedsim::algo {

class EdfScheduler;

/// @brief Constant Bandwidth Server (CBS) implementation.
/// @ingroup algo_servers
///
/// A CbsServer reserves a fraction of processor bandwidth (budget/period) for
/// a single task. It maintains a FIFO job queue, tracks a scheduling deadline
/// used by EDF, and implements the CBS state machine for budget replenishment,
/// exhaustion, and optional GRUB non-contending behaviour.
///
/// Servers are non-copyable but movable so they can be stored in containers.
///
/// @see EdfScheduler, Scheduler, GrubPolicy
class CbsServer {
public:
    /// @brief Server state machine states.
    enum class State {
        Inactive,      ///< No pending jobs; server is dormant.
        Ready,         ///< Has pending jobs, waiting to be dispatched by EDF.
        Running,       ///< Currently executing on a processor.
        NonContending  ///< GRUB: job completed early, waiting for deadline expiry.
    };

    /// @brief Policy for handling a new job arrival while the server is already active.
    enum class OverrunPolicy {
        Queue,  ///< Queue the new job behind the current one (default).
        Skip,   ///< Drop the new job silently.
        Abort   ///< Abort the current job and start the new one immediately.
    };

    /// @brief Construct a CBS server with the given bandwidth reservation.
    ///
    /// @param id     Unique identifier (used for deterministic tie-breaking in EDF).
    /// @param budget Maximum execution budget per period (Q).
    /// @param period Replenishment period (T). Utilization = Q/T.
    /// @param policy How to handle a new arrival when the server is active.
    CbsServer(std::size_t id, core::Duration budget, core::Duration period,
              OverrunPolicy policy = OverrunPolicy::Queue);

    /// @brief Get the server's unique identifier.
    /// @return Server ID used for deterministic EDF tie-breaking.
    [[nodiscard]] std::size_t id() const noexcept { return id_; }

    /// @cond INTERNAL
    CbsServer(const CbsServer&) = delete;
    CbsServer& operator=(const CbsServer&) = delete;
    CbsServer(CbsServer&&) = default;
    CbsServer& operator=(CbsServer&&) = default;
    /// @endcond

    ~CbsServer() = default;

    // -------------------------------------------------------------------
    // CBS parameters
    // -------------------------------------------------------------------

    /// @brief Get the maximum execution budget per period (Q).
    /// @return The server budget as a Duration.
    [[nodiscard]] core::Duration budget() const noexcept { return budget_; }

    /// @brief Get the replenishment period (T).
    /// @return The server period as a Duration.
    [[nodiscard]] core::Duration period() const noexcept { return period_; }

    /// @brief Get the server utilization (Q / T).
    /// @return Utilization as a dimensionless ratio in [0, 1].
    [[nodiscard]] double utilization() const noexcept { return utilization_; }

    // -------------------------------------------------------------------
    // State accessors
    // -------------------------------------------------------------------

    /// @brief Get the current server state.
    /// @return One of Inactive, Ready, Running, or NonContending.
    /// @see State
    [[nodiscard]] State state() const noexcept { return state_; }

    /// @brief Get the current scheduling deadline.
    /// @return The absolute deadline used by EDF for priority ordering.
    [[nodiscard]] core::TimePoint deadline() const noexcept { return deadline_; }

    /// @brief Get the current virtual time.
    ///
    /// Virtual time tracks logical progress and is used by GRUB for
    /// reclamation accounting.
    ///
    /// @return The server's virtual time.
    /// @see update_virtual_time, set_virtual_time
    [[nodiscard]] core::TimePoint virtual_time() const noexcept { return virtual_time_; }

    /// @brief Get the remaining execution budget.
    /// @return Budget left before the server must be replenished.
    [[nodiscard]] core::Duration remaining_budget() const noexcept { return remaining_budget_; }

    /// @brief Get the overrun policy for this server.
    /// @return The OverrunPolicy set at construction.
    /// @see OverrunPolicy
    [[nodiscard]] OverrunPolicy overrun_policy() const noexcept { return overrun_policy_; }

    // -------------------------------------------------------------------
    // Job queue accessors
    // -------------------------------------------------------------------

    /// @brief Check whether the server has pending jobs.
    /// @return True if the job queue is non-empty.
    [[nodiscard]] bool has_pending_jobs() const noexcept { return !job_queue_.empty(); }

    /// @brief Get the number of jobs waiting in the queue.
    /// @return Queue depth.
    [[nodiscard]] std::size_t job_queue_size() const noexcept { return job_queue_.size(); }

    /// @brief Get the ID of the most recently enqueued job.
    /// @return Job ID (monotonically increasing per server).
    [[nodiscard]] uint64_t last_enqueued_job_id() const noexcept { return last_enqueued_job_id_; }

    /// @brief Get a pointer to the job at the head of the queue.
    /// @return Pointer to the current job, or nullptr if the queue is empty.
    [[nodiscard]] core::Job* current_job();

    /// @brief Get a const pointer to the job at the head of the queue.
    /// @return Const pointer to the current job, or nullptr if the queue is empty.
    [[nodiscard]] const core::Job* current_job() const;

    // -------------------------------------------------------------------
    // Job queue management
    // -------------------------------------------------------------------

    /// @brief Enqueue a new job at the back of the server's FIFO queue.
    /// @param job The job to enqueue.
    void enqueue_job(core::Job job);

    /// @brief Dequeue and return the job at the front of the queue.
    /// @return The dequeued job.
    /// @throws std::logic_error if the queue is empty.
    core::Job dequeue_job();

    // -------------------------------------------------------------------
    // State transitions
    // -------------------------------------------------------------------

    /// @brief Activate the server (Inactive -> Ready).
    ///
    /// Called when a job arrives for an inactive server. Initialises the
    /// deadline and virtual time based on the current simulation time.
    ///
    /// @param current_time The current simulation time.
    void activate(core::TimePoint current_time);

    /// @brief Dispatch the server (Ready -> Running).
    ///
    /// Called by EDF when this server is selected for execution on a processor.
    void dispatch();

    /// @brief Preempt the server (Running -> Ready).
    ///
    /// Called when a higher-priority server preempts this one.
    void preempt();

    /// @brief Complete the current job (Running -> Ready or Inactive).
    ///
    /// Transitions to Ready if further jobs are queued, or to Inactive
    /// if the queue is empty.
    ///
    /// @param current_time The current simulation time.
    void complete_job(core::TimePoint current_time);

    /// @brief Abort the head job from the queue without executing it.
    ///
    /// Used for queued deadline-miss handling. Transitions to Inactive if
    /// the queue becomes empty after removal.
    void abort_queued_job();

    /// @brief Handle budget exhaustion (Running -> Ready).
    ///
    /// Called when the remaining budget reaches zero. Postpones the
    /// deadline and replenishes the budget.
    ///
    /// @param current_time The current simulation time.
    /// @see postpone_deadline
    void exhaust_budget(core::TimePoint current_time);

    /// @brief Enter non-contending state (Running -> NonContending).
    ///
    /// GRUB extension: the job completed early and the server waits for
    /// its deadline to expire before releasing its bandwidth.
    ///
    /// @param current_time The current simulation time.
    /// @see GrubPolicy
    void enter_non_contending(core::TimePoint current_time);

    /// @brief Reactivate from non-contending (NonContending -> Ready).
    ///
    /// GRUB extension: a new job arrives while the server is waiting for
    /// its deadline, so it re-enters the ready queue.
    void reactivate_from_non_contending();

    /// @brief Handle deadline expiry in non-contending state (NonContending -> Inactive).
    ///
    /// GRUB extension: the server's deadline is reached while non-contending,
    /// so it becomes inactive and releases its bandwidth.
    ///
    /// @param current_time The current simulation time.
    void reach_deadline(core::TimePoint current_time);

    // -------------------------------------------------------------------
    // CBS formulas
    // -------------------------------------------------------------------

    /// @brief Update virtual time after execution.
    ///
    /// Advances virtual time by execution_time / utilization, reflecting
    /// the logical time consumed by the server.
    ///
    /// @param execution_time The wall-clock execution time to account for.
    /// @see set_virtual_time
    void update_virtual_time(core::Duration execution_time);

    /// @brief Set the virtual time directly.
    ///
    /// Used when a reclamation policy (e.g., GRUB) computes virtual time
    /// externally.
    ///
    /// @param vt The new virtual time value.
    /// @see update_virtual_time
    void set_virtual_time(core::TimePoint vt) noexcept { virtual_time_ = vt; }

    /// @brief Postpone the deadline and replenish the budget.
    ///
    /// Advances the deadline by one period (d += T) and resets the
    /// remaining budget to the full budget (remaining = Q).
    void postpone_deadline();

    /// @brief Consume a portion of the remaining budget.
    /// @param amount The amount of budget to subtract.
    void consume_budget(core::Duration amount);

    // -------------------------------------------------------------------
    // Task association
    // -------------------------------------------------------------------

    /// @brief Get the task associated with this server.
    ///
    /// The association is established by EdfScheduler when the server is
    /// registered for a task.
    ///
    /// @return Pointer to the associated task, or nullptr if none.
    /// @see EdfScheduler
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
    core::TimePoint deadline_{};
    core::TimePoint virtual_time_{};
    core::Duration remaining_budget_{};
    uint64_t job_counter_{0};
    uint64_t last_enqueued_job_id_{0};

    std::deque<core::Job> job_queue_;
    core::Task* task_{nullptr};
    EdfScheduler* scheduler_{nullptr};
};

} // namespace schedsim::algo
