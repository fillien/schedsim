#pragma once

#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/types.hpp>

#include <cstddef>

namespace schedsim::algo {

/// @brief Abstract interface for scheduling algorithms.
/// @ingroup algo_schedulers
///
/// A Scheduler manages a set of processors and makes dispatch decisions.
/// Concrete implementations (e.g., EdfScheduler) maintain a ready queue of
/// CBS servers and assign them to processors according to a scheduling policy.
///
/// @see EdfScheduler, CbsServer, Allocator
class Scheduler {
public:
    /// @brief Handle a job arrival for a task.
    ///
    /// The scheduler should find or create a server for the task and enqueue
    /// the job. This is the main entry point called by the allocator or
    /// directly by the simulation engine on each job release.
    ///
    /// @param task The task that released the job.
    /// @param job  The newly released job to be scheduled.
    /// @see Allocator::on_job_arrival
    virtual void on_job_arrival(core::Task& task, core::Job job) = 0;

    /// @brief Check if a new server with the given budget/period can be admitted.
    ///
    /// Performs an admission test to determine whether adding a server with
    /// utilization budget/period would exceed the scheduler's capacity bound.
    ///
    /// @param budget The maximum execution budget per period.
    /// @param period The replenishment period.
    /// @return True if the server can be admitted without violating the
    ///         utilization bound; false otherwise.
    /// @see AdmissionTest, AdmissionError
    virtual bool can_admit(core::Duration budget, core::Duration period) const = 0;

    /// @brief Get the total server utilization of this scheduler.
    ///
    /// Returns the sum of utilizations (budget/period) of all servers
    /// currently registered with this scheduler.
    ///
    /// @return Total utilization as a dimensionless ratio.
    virtual double utilization() const = 0;

    /// @brief Get the number of processors managed by this scheduler.
    /// @return The processor count.
    virtual std::size_t processor_count() const noexcept = 0;

    /// @brief Set the expected number of job arrivals for a task.
    ///
    /// Used for server detach tracking: when a task has released all its
    /// expected jobs and they have completed, the scheduler may detach the
    /// associated server to reclaim bandwidth. The default implementation
    /// is a no-op.
    ///
    /// @param task  The task whose expected arrival count is being set.
    /// @param count The total number of jobs expected from this task.
    virtual void set_expected_arrivals(const core::Task& task, std::size_t count) {}

    virtual ~Scheduler() = default;
};

} // namespace schedsim::algo
