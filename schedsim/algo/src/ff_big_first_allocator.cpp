#include <schedsim/algo/ff_big_first_allocator.hpp>

#include <algorithm>
#include <vector>

namespace schedsim::algo {

Cluster* FFBigFirstAllocator::select_cluster(const core::Task& task) {
    std::vector<Cluster*> sorted(clusters().begin(), clusters().end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Cluster* a, const Cluster* b) {
                  return a->perf() > b->perf();  // descending
              });

    for (auto* cluster : sorted) {
        if (cluster->can_admit(task.wcet(), task.period())) {
            return cluster;
        }
    }
    return nullptr;
}

} // namespace schedsim::algo
