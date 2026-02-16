#pragma once

#include <schedsim/core/task.hpp>

namespace schedsim::algo {

inline double task_utilization(const core::Task& t) {
    return core::duration_ratio(t.wcet(), t.period());
}

} // namespace schedsim::algo
