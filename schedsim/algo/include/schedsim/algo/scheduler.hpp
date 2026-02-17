#pragma once

#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/types.hpp>

#include <cstddef>

namespace schedsim::algo {

// Abstract interface for scheduling algorithms
// A scheduler manages a set of processors and makes dispatch decisions
class Scheduler {
public:
    // Handle a job arrival for a task
    // The scheduler should find or create a server for the task and enqueue the job
    virtual void on_job_arrival(core::Task& task, core::Job job) = 0;

    // Check if a new server with given budget/period can be admitted
    // Returns true if admission is possible without exceeding utilization bounds
    virtual bool can_admit(core::Duration budget, core::Duration period) const = 0;

    // Get the total server utilization of this scheduler
    virtual double utilization() const = 0;

    // Get the number of processors managed by this scheduler
    virtual std::size_t processor_count() const noexcept = 0;

    // Set expected number of job arrivals for a task (for server detach tracking)
    virtual void set_expected_arrivals(const core::Task& /*task*/, std::size_t /*count*/) {}

    virtual ~Scheduler() = default;
};

} // namespace schedsim::algo
