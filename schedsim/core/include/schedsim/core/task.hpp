#pragma once

#include <schedsim/core/types.hpp>

#include <cstddef>

namespace schedsim::core {

class ProcessorType;

/// @brief Represents a periodic or sporadic real-time task in the system.
/// @ingroup core
///
/// WCET is specified in reference units, normalized to the highest-performance
/// processor type in the platform. Actual wall-clock execution time depends on
/// the processor speed and is obtained via the overloaded wcet(ProcessorType&, double)
/// method.
///
/// Tasks are non-copyable but movable, following Decision 61.
///
/// @see Job, ProcessorType, Platform
class Task {
public:
    /// @brief Construct a new Task.
    /// @param id        Unique task identifier (1-based in scenario JSON).
    /// @param period    Minimum inter-arrival time between consecutive jobs.
    /// @param relative_deadline  Relative deadline from job arrival to completion.
    /// @param wcet      Worst-case execution time in reference (normalized) units.
    Task(std::size_t id, Duration period, Duration relative_deadline, Duration wcet);

    /// @brief Get the unique task identifier.
    /// @return Task ID.
    [[nodiscard]] std::size_t id() const noexcept { return id_; }

    /// @brief Get the task period (minimum inter-arrival time).
    /// @return Task period as a Duration.
    [[nodiscard]] Duration period() const noexcept { return period_; }

    /// @brief Get the relative deadline.
    /// @return Relative deadline as a Duration from job arrival.
    [[nodiscard]] Duration relative_deadline() const noexcept { return relative_deadline_; }

    /// @brief Get the worst-case execution time in reference units.
    /// @return WCET as a Duration in normalized reference units.
    /// @see wcet(const ProcessorType&, double) for wall-clock WCET on a specific processor.
    [[nodiscard]] Duration wcet() const noexcept { return wcet_; }

    /// @brief Compute the wall-clock WCET for a specific processor type.
    ///
    /// Scales the reference WCET by the ratio of reference performance to the
    /// target processor type's performance:
    ///   wall_clock_wcet = wcet / (type.performance() / reference_performance)
    ///
    /// @param type                  The target processor type.
    /// @param reference_performance The reference (highest) performance value in the platform.
    /// @return Wall-clock WCET as a Duration for the given processor type.
    /// @see ProcessorType::performance()
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
