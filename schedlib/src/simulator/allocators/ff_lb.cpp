#include <print>
#include <simulator/allocator.hpp>
#include <simulator/allocators/ff_lb.hpp>
#include <simulator/scheduler.hpp>

#include <algorithm>
#include <memory>
#include <optional>

auto allocators::FirstFitLoadBalancer::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() < second->cluster()->perf();
        };

        // Sort the schedulers by perf score
        auto sorted_scheds{schedulers()};
        std::ranges::sort(sorted_scheds, compare_perf);

        std::optional<std::shared_ptr<scheds::Scheduler>> next_sched;

        const auto avg_big_util =
            sorted_scheds.back()->total_utilization() /
            static_cast<double>(sorted_scheds.back()->cluster()->processors().size());

        // Look for a cluster to place the task
        for (auto& sched : sorted_scheds) {
                const auto& clu = sched->cluster();
                if (sched != sorted_scheds.back()) { clu->u_target(avg_big_util * clu->perf()); }
                if (((new_task->utilization() * (clu->scale_speed()) / clu->perf())) <=
                    clu->u_target()) {
                        if (sched->admission_test(*new_task)) {
                                next_sched = sched;
                                break;
                        }
                }
        }

        // Otherwise return that the allocator didn't found a cluster to place the task
        return next_sched;
}
