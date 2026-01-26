#include <print>
#include <simulator/allocator.hpp>
#include <simulator/allocators/ff_lb.hpp>
#include <simulator/scheduler.hpp>

#include <algorithm>
#include <vector>

auto allocators::FirstFitLoadBalancer::where_to_put_the_task(const Task& new_task)
    -> scheds::Scheduler*
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() < second->cluster()->perf();
        };

        // Sort the schedulers by perf score
        std::vector<scheds::Scheduler*> sorted_scheds;
        sorted_scheds.reserve(schedulers().size());
        for (const auto& s : schedulers()) sorted_scheds.push_back(s.get());
        std::ranges::sort(sorted_scheds, compare_perf);

        const auto avg_big_util =
            sorted_scheds.back()->total_utilization() /
            static_cast<double>(sorted_scheds.back()->cluster()->processors().size());

        // Look for a cluster to place the task
        for (auto* sched : sorted_scheds) {
                auto* clu = sched->cluster();
                if (sched != sorted_scheds.back()) { clu->u_target(avg_big_util * clu->perf()); }
                if (((new_task.utilization() * (clu->scale_speed()) / clu->perf())) <=
                    clu->u_target()) {
                        if (sched->admission_test(new_task)) { return sched; }
                }
        }

        // Otherwise return that the allocator didn't find a cluster to place the task
        return nullptr;
}
