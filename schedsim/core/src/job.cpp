#include <schedsim/core/job.hpp>
#include <schedsim/core/error.hpp>

namespace schedsim::core {

Job::Job(Task& task, Duration total_work, TimePoint absolute_deadline)
    : task_(&task)
    , remaining_work_(total_work)
    , total_work_(total_work)
    , absolute_deadline_(absolute_deadline) {}

void Job::consume_work(Duration amount) {
    remaining_work_ -= amount;
    if (remaining_work_.count() < 0.0) {
        // Clamp to zero: negative remaining work is always floating-point error
        // from accumulated DVFS speed changes (fractional freq ratios like
        // 200/1400) and heterogeneous performance scaling.
        remaining_work_ = Duration{0.0};
    }
}

} // namespace schedsim::core
