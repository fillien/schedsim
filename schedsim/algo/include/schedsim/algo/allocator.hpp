#pragma once

#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>

namespace schedsim::algo {

/// @brief Abstract interface for job allocation.
/// @ingroup algo_allocators
///
/// An Allocator routes incoming jobs to the appropriate Scheduler.
/// Concrete implementations decide the mapping strategy (e.g., partitioned
/// worst-fit, best-fit, or global).
///
/// @see Scheduler, Cluster
class Allocator {
public:
    /// @brief Handle a job arrival for a task.
    ///
    /// The allocator decides which scheduler should handle this job and
    /// forwards it accordingly.
    ///
    /// @param task The task that released the job.
    /// @param job  The newly released job to be routed.
    /// @throws AllocationError if no suitable scheduler can accept the job.
    /// @see Scheduler::on_job_arrival
    virtual void on_job_arrival(core::Task& task, core::Job job) = 0;

    virtual ~Allocator() = default;
};

} // namespace schedsim::algo
