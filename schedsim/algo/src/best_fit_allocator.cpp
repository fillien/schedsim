#include <schedsim/algo/best_fit_allocator.hpp>
#include <schedsim/algo/task_utils.hpp>

#include <limits>

namespace schedsim::algo {

Cluster* BestFitAllocator::select_cluster(const core::Task& task) {
    double util = task_utilization(task);
    Cluster* best = nullptr;
    double best_remaining = std::numeric_limits<double>::max();

    for (auto* cluster : clusters()) {
        if (cluster->scaled_utilization(util) <= cluster->u_target() &&
            cluster->can_admit(task.wcet(), task.period())) {
            double remaining = cluster->remaining_capacity();
            if (remaining < best_remaining) {
                best_remaining = remaining;
                best = cluster;
            }
        }
    }
    return best;
}

} // namespace schedsim::algo
