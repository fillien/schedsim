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

// Simple allocator that routes all jobs to a single scheduler
// Useful for global scheduling or single-scheduler setups
class SingleSchedulerAllocator : public Allocator {
public:
    SingleSchedulerAllocator(core::Engine& engine, Scheduler& scheduler,
                             core::ClockDomain* clock_domain = nullptr);
    ~SingleSchedulerAllocator() override = default;

    // Non-copyable, non-movable (registered with engine)
    SingleSchedulerAllocator(const SingleSchedulerAllocator&) = delete;
    SingleSchedulerAllocator& operator=(const SingleSchedulerAllocator&) = delete;
    SingleSchedulerAllocator(SingleSchedulerAllocator&&) = delete;
    SingleSchedulerAllocator& operator=(SingleSchedulerAllocator&&) = delete;

    void on_job_arrival(core::Task& task, core::Job job) override;

private:
    core::Engine& engine_;
    Scheduler& scheduler_;
    core::ClockDomain* clock_domain_;
    std::unordered_set<core::Task*> placed_tasks_;
};

} // namespace schedsim::algo
