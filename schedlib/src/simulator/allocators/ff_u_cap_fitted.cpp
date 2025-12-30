#include <simulator/allocators/ff_u_cap_fitted.hpp>

#include <simulator/scheduler.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>

auto allocators::FFUCapFitted::where_to_put_the_task(
    const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        // TODO: Implement fitted utilization model
        // For now, fall back to simple first-fit behavior

        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() < second->cluster()->perf();
        };

        auto sorted_scheds{schedulers()};
        std::ranges::sort(sorted_scheds, compare_perf);

        std::optional<std::shared_ptr<scheds::Scheduler>> next_sched;

        for (auto& sched : sorted_scheds) {
                if (sched->admission_test(*new_task)) {
                        next_sched = sched;
                        break;
                }
        }

        return next_sched;
}
