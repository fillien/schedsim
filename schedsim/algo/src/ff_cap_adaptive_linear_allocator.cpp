#include <schedsim/algo/ff_cap_adaptive_linear_allocator.hpp>
#include <schedsim/algo/task_utils.hpp>

#include <algorithm>
#include <vector>

namespace schedsim::algo {

namespace {
constexpr double A_UMAX = 1.616;
constexpr double B_U    = 0.098;
constexpr double C      = -0.373;

double linear_model(double umax, double total_util) {
    return std::clamp(A_UMAX * umax + B_U * total_util + C, 0.0, 1.0);
}
} // namespace

Cluster* FFCapAdaptiveLinearAllocator::select_cluster(const core::Task& task) {
    double util = task_utilization(task);
    observed_umax_ = std::max(observed_umax_, util);

    std::vector<Cluster*> sorted(clusters().begin(), clusters().end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Cluster* a, const Cluster* b) {
                  return a->perf() < b->perf();  // ascending
              });

    // Apply model to the smallest (first) cluster
    if (!sorted.empty()) {
        sorted[0]->set_u_target(linear_model(observed_umax_, expected_total_util_));
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
