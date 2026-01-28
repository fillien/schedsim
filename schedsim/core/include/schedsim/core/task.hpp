#pragma once

#include <schedsim/core/types.hpp>

#include <cstddef>

namespace schedsim::core {

class ProcessorType;

// Task represents a periodic or sporadic task in the system.
// WCET is specified in reference units (normalized to the highest-performance
// processor type). Actual wall-clock execution time depends on processor speed.
class Task {
public:
    Task(std::size_t id, Duration period, Duration relative_deadline, Duration wcet);

    [[nodiscard]] std::size_t id() const noexcept { return id_; }
    [[nodiscard]] Duration period() const noexcept { return period_; }
    [[nodiscard]] Duration relative_deadline() const noexcept { return relative_deadline_; }
    [[nodiscard]] Duration wcet() const noexcept { return wcet_; }

    // Compute wall-clock WCET for a specific processor type.
    // Formula: wcet / (type.performance / reference_performance)
    // This gives the actual execution time on that processor type.
    [[nodiscard]] Duration wcet(const ProcessorType& type,
                                double reference_performance) const noexcept;

    // Non-copyable, movable (Decision 61)
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&&) = default;
    Task& operator=(Task&&) = default;

private:
    std::size_t id_;
    Duration period_;
    Duration relative_deadline_;
    Duration wcet_;
};

} // namespace schedsim::core
