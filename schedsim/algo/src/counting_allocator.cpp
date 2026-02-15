#include <schedsim/algo/counting_allocator.hpp>

namespace schedsim::algo {

Cluster* CountingAllocator::select_cluster(const core::Task& task) {
    ++count_;

    for (auto* cluster : clusters()) {
        if (cluster->can_admit(task.wcet(), task.period())) {
            return cluster;
        }
    }
    return nullptr;
}

} // namespace schedsim::algo
