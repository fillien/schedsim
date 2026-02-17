#include <schedsim/algo/worst_fit_allocator.hpp>
#include <schedsim/algo/task_utils.hpp>

namespace schedsim::algo {

Cluster* WorstFitAllocator::select_cluster(const core::Task& task) {
    double util = task_utilization(task);
    Cluster* best = nullptr;
    double best_remaining = -1.0;

    for (auto* cluster : clusters()) {
        if (cluster->scaled_utilization(util) <= cluster->u_target() &&
            cluster->can_admit(task.wcet(), task.period())) {
            double remaining = cluster->remaining_capacity();
            if (remaining > best_remaining) {
                best_remaining = remaining;
                best = cluster;
            }
        }
    }
    return best;
}

} // namespace schedsim::algo
