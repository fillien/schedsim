#include <schedsim/algo/ff_cap_allocator.hpp>
#include <schedsim/algo/task_utils.hpp>

#include <algorithm>
#include <vector>

namespace schedsim::algo {

Cluster* FFCapAllocator::select_cluster(const core::Task& task) {
    double util = task_utilization(task);

    std::vector<Cluster*> sorted(clusters().begin(), clusters().end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Cluster* a, const Cluster* b) {
                  return a->perf() < b->perf();  // ascending
              });

    for (auto* cluster : sorted) {
        if (cluster->try_admit_scaled(util)) {
            return cluster;
        }
    }
    return nullptr;
}

} // namespace schedsim::algo
