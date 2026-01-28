#pragma once

#include <schedsim/core/types.hpp>

namespace schedsim::core {

class Task;

// Job represents a single instance of a task.
// Created by the Engine at job arrival time, owned by Library 2 (allocator).
// Work is tracked in reference units (normalized execution time).
class Job {
public:
    Job(Task& task, Duration total_work, TimePoint absolute_deadline);

    [[nodiscard]] Task& task() const noexcept { return *task_; }
    [[nodiscard]] Duration remaining_work() const noexcept { return remaining_work_; }
    [[nodiscard]] Duration total_work() const noexcept { return total_work_; }
    [[nodiscard]] TimePoint absolute_deadline() const noexcept { return absolute_deadline_; }
    [[nodiscard]] bool is_complete() const noexcept { return remaining_work_.count() <= 0.0; }

    // Called by Processor during execution tracking.
    // Amount is in reference units.
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
