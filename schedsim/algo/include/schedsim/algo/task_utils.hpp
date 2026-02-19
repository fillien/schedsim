#pragma once

/// @file task_utils.hpp
/// @brief Task-level utility functions for the algo library.
/// @ingroup algo_policies

#include <schedsim/core/task.hpp>

namespace schedsim::algo {

/// @brief Compute the utilization of a task as WCET / Period.
///
/// Returns the ratio of the task's worst-case execution time to its period,
/// both expressed as core::Duration values. The result is a dimensionless
/// value in [0, 1] for well-formed task sets.
///
/// @param t The task whose utilization is computed.
/// @return The utilization ratio (WCET / Period) as a double.
///
/// @see core::Task, EdfScheduler::utilization
inline double task_utilization(const core::Task& t) {
    return core::duration_ratio(t.wcet(), t.period());
}

} // namespace schedsim::algo
