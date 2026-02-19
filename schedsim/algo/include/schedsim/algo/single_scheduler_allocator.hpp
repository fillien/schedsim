#pragma once

#include <schedsim/algo/allocator.hpp>
#include <schedsim/algo/scheduler.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>

#include <unordered_set>

namespace schedsim::core {
class ClockDomain;
} // namespace schedsim::core

namespace schedsim::algo {

/// @brief Allocator that routes all jobs to a single Scheduler instance.
///
/// This is the simplest allocator strategy: every task is placed on the
/// same scheduler, making it suitable for global scheduling policies
/// (e.g. global EDF on a single clock domain) or single-scheduler setups.
///
/// On the first job arrival for a given task, the task is registered with
/// the scheduler (and optionally with a ClockDomain for DVFS-aware
/// accounting).  Subsequent jobs for the same task are forwarded directly.
///
/// @ingroup algo_allocators
/// @see Allocator, Scheduler, EdfScheduler, Cluster
class SingleSchedulerAllocator : public Allocator {
public:
    /// @brief Construct a single-scheduler allocator.
    /// @param engine        Reference to the simulation engine.
    /// @param scheduler     The unique scheduler that will receive all jobs.
    /// @param clock_domain  Optional clock domain for DVFS-aware task
    ///                      placement.  Pass @c nullptr when DVFS is unused.
    SingleSchedulerAllocator(core::Engine& engine, Scheduler& scheduler,
                             core::ClockDomain* clock_domain = nullptr);

    /// @brief Virtual destructor.
    ~SingleSchedulerAllocator() override = default;

    /// Non-copyable, non-movable (registered with engine)
    SingleSchedulerAllocator(const SingleSchedulerAllocator&) = delete;
    SingleSchedulerAllocator& operator=(const SingleSchedulerAllocator&) = delete;
    SingleSchedulerAllocator(SingleSchedulerAllocator&&) = delete;
    SingleSchedulerAllocator& operator=(SingleSchedulerAllocator&&) = delete;

    /// @brief Handle a job arrival for a task.
    ///
    /// On the first arrival the task is placed on the scheduler (and
    /// optionally linked to the clock domain).  Every arrival then
    /// forwards the job to the scheduler.
    ///
    /// @param task  The task that produced the job.
    /// @param job   The newly arrived job to dispatch.
    /// @see Allocator::on_job_arrival
    void on_job_arrival(core::Task& task, core::Job job) override;

private:
    core::Engine& engine_;
    Scheduler& scheduler_;
    core::ClockDomain* clock_domain_;
    std::unordered_set<core::Task*> placed_tasks_;
};

} // namespace schedsim::algo
