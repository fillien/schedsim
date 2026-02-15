#include <schedsim/algo/ff_lb_allocator.hpp>
#include <schedsim/algo/task_utils.hpp>

#include <algorithm>
#include <vector>

namespace schedsim::algo {

Cluster* FFLbAllocator::select_cluster(const core::Task& task) {
    double util = task_utilization(task);

    std::vector<Cluster*> sorted(clusters().begin(), clusters().end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Cluster* a, const Cluster* b) {
                  return a->perf() < b->perf();  // ascending
              });

    if (sorted.empty()) {
        return nullptr;
    }

    // Big cluster is the last (highest perf)
    Cluster* big = sorted.back();
    // Use raw utilization (sum of server budget/period), matching legacy total_utilization()
    double avg_big = (big->processor_count() > 0)
        ? big->utilization() / static_cast<double>(big->processor_count())
        : 0.0;

    // Set u_target on non-big clusters proportional to their perf
    for (auto* cluster : sorted) {
        if (cluster != big) {
            cluster->set_u_target(avg_big * cluster->perf());
        }
    }

    for (auto* cluster : sorted) {
        if (cluster->scaled_utilization(util) <= cluster->u_target() &&
            cluster->can_admit(task.wcet(), task.period())) {
            return cluster;
        }
    }
    return nullptr;
}

} // namespace schedsim::algo
