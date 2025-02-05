#include <algorithm>
#include <allocator.hpp>
#include <allocators/smart_ass.hpp>
#include <optional>
#include <scheduler.hpp>

auto allocators::smart_ass::where_to_put_the_task(const std::shared_ptr<task>& new_task)
    -> std::optional<std::shared_ptr<scheds::scheduler>>
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->get_cluster()->perf() < second->get_cluster()->perf();
        };

        // Sort the schedulers by perf score
        auto sorted_scheds{schedulers};
        std::sort(sorted_scheds.begin(), sorted_scheds.end(), compare_perf);

        // Look for a cluster to place the task
        for (auto& sched : sorted_scheds) {
                if (std::max(new_task->utilization, sched->get_umax()) <
                        sched->get_cluster()->perf() &&
                    sched->admission_test(*new_task)) {
                        return sched;
                }
        }

        // If no scheduler have been found, the allocator try to place the task on the most
        // powerfull cluster
        if (schedulers.at(0)->admission_test(*new_task)) {
                return schedulers.at(0)->shared_from_this();
        }

        // Otherwise return that the allocator didn't found a cluster to place the task
        return {};
}
