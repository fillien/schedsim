#pragma once

#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>

namespace schedsim::algo {

// Abstract interface for job allocation
// An allocator routes incoming jobs to the appropriate scheduler
class Allocator {
public:
    // Handle a job arrival for a task
    // The allocator decides which scheduler should handle this job
    virtual void on_job_arrival(core::Task& task, core::Job job) = 0;

    virtual ~Allocator() = default;
};

} // namespace schedsim::algo
