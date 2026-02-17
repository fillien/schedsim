#include <schedsim/algo/first_fit_allocator.hpp>
#include <schedsim/algo/task_utils.hpp>

namespace schedsim::algo {

Cluster* FirstFitAllocator::select_cluster(const core::Task& task) {
    double util = task_utilization(task);
    for (auto* cluster : clusters()) {
        if (cluster->scaled_utilization(util) <= cluster->u_target() &&
            cluster->can_admit(task.wcet(), task.period())) {
            return cluster;
        }
    }
    return nullptr;
}

} // namespace schedsim::algo
