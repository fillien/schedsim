#include <schedsim/algo/ff_cap_adaptive_poly_allocator.hpp>
#include <schedsim/algo/task_utils.hpp>

#include <algorithm>
#include <vector>

namespace schedsim::algo {

namespace {
// Polynomial model: target = C0 + C1*umax + C2*U + C3*umax² + C4*umax*U + C5*U²
// R² = 0.9752, MAE = 0.031259
constexpr double C0 = -3.556483715;
constexpr double C1 =  0.465435461;
constexpr double C2 =  1.273160775;
constexpr double C3 = -2.805216317;
constexpr double C4 =  0.651635123;
constexpr double C5 = -0.112785097;

double poly_model(double umax, double total_util) {
    return std::clamp(
        C0 + C1 * umax + C2 * total_util
           + C3 * umax * umax + C4 * umax * total_util
           + C5 * total_util * total_util,
        0.0, 1.0);
}
} // namespace

Cluster* FFCapAdaptivePolyAllocator::select_cluster(const core::Task& task) {
    double util = task_utilization(task);
    observed_umax_ = std::max(observed_umax_, util);

    std::vector<Cluster*> sorted(clusters().begin(), clusters().end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Cluster* a, const Cluster* b) {
                  return a->perf() < b->perf();  // ascending
              });

    // Apply model to the smallest (first) cluster
    if (!sorted.empty()) {
        sorted[0]->set_u_target(poly_model(observed_umax_, expected_total_util_));
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
