#pragma once

#include <schedsim/core/types.hpp>

namespace schedsim::core {

class Task;

/// @brief Represents a single instance (release) of a Task.
/// @ingroup core
///
/// A Job is created by the Engine at job arrival time and is owned by the
/// allocator (Library 2). Work is tracked in reference units (normalized
/// execution time), independent of the processor on which the job executes.
///
/// Jobs are non-copyable but movable, following Decision 73.
///
/// @see Task, Processor, Engine
class Job {
public:
    /// @brief Construct a new Job.
    /// @param task              Reference to the parent Task that spawned this job.
    /// @param total_work        Total amount of work for this job in reference units.
    /// @param absolute_deadline Absolute deadline by which the job must complete.
    Job(Task& task, Duration total_work, TimePoint absolute_deadline);

    /// @brief Get the parent task.
    /// @return Reference to the Task that this job belongs to.
    /// @see Task
    [[nodiscard]] Task& task() const noexcept { return *task_; }

    /// @brief Get the remaining work for this job.
    /// @return Remaining work as a Duration in reference units.
    [[nodiscard]] Duration remaining_work() const noexcept { return remaining_work_; }

    /// @brief Get the total work for this job.
    /// @return Total work as a Duration in reference units.
    [[nodiscard]] Duration total_work() const noexcept { return total_work_; }

    /// @brief Get the absolute deadline.
    /// @return Absolute deadline as a TimePoint.
    [[nodiscard]] TimePoint absolute_deadline() const noexcept { return absolute_deadline_; }

    /// @brief Check whether the job has completed all of its work.
    /// @return True if remaining work is zero or less, false otherwise.
    [[nodiscard]] bool is_complete() const noexcept { return remaining_work_ <= Duration::zero(); }

    /// @brief Consume a given amount of work from this job.
    ///
    /// Called by Processor during execution tracking. The amount is specified
    /// in reference units and is subtracted from the remaining work. The
    /// remaining work is clamped so it does not go below zero.
    ///
    /// @param amount Amount of work to consume, in reference units.
    /// @see Processor
    void consume_work(Duration amount);

    // Movable, not copyable (Decision 73)
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    Job(Job&&) = default;
    Job& operator=(Job&&) = default;

private:
    Task* task_;
    Duration remaining_work_;
    Duration total_work_;
    TimePoint absolute_deadline_;
};

} // namespace schedsim::core
