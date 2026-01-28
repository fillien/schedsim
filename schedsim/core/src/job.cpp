#include <schedsim/core/job.hpp>

namespace schedsim::core {

Job::Job(Task& task, Duration total_work, TimePoint absolute_deadline)
    : task_(&task)
    , remaining_work_(total_work)
    , total_work_(total_work)
    , absolute_deadline_(absolute_deadline) {}

void Job::consume_work(Duration amount) {
    remaining_work_ -= amount;
}

} // namespace schedsim::core
