#include <algorithm>
#include <allocator.hpp>
#include <allocators/smart_ass.hpp>
#include <memory>
#include <optional>
#include <scheduler.hpp>

auto allocators::SmartAss::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->get_cluster()->perf() < second->get_cluster()->perf();
        };

        // Sort the schedulers by perf score
        auto sorted_scheds{schedulers};
        std::sort(sorted_scheds.begin(), sorted_scheds.end(), compare_perf);

        std::optional<std::shared_ptr<scheds::Scheduler>> next_sched;

        // Look for a cluster to place the task
        for (auto& sched : sorted_scheds) {
                const auto clu = sched->get_cluster();
                if (new_task->utilization / clu->perf() < sched->u_max() / clu->perf()) {
                        if (sched->admission_test(*new_task)) {
                                next_sched = sched;
                                break;
                        }
                }
        }

        // Otherwise return that the allocator didn't found a cluster to place the task
        return next_sched;
}
