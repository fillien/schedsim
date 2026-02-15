#pragma once

#include <schedsim/core/task.hpp>

namespace schedsim::algo {

inline double task_utilization(const core::Task& t) {
    return t.wcet().count() / t.period().count();
}

} // namespace schedsim::algo
