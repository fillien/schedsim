#include <simulator/allocators/ff_u_cap_fitted.hpp>

#include <simulator/scheduler.hpp>

#include <algorithm>
#include <vector>

auto allocators::FFUCapFitted::where_to_put_the_task(const Task& new_task) -> scheds::Scheduler*
{
        // TODO: Implement fitted utilization model
        // For now, fall back to simple first-fit behavior

        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() < second->cluster()->perf();
        };

        std::vector<scheds::Scheduler*> sorted_scheds;
        sorted_scheds.reserve(schedulers().size());
        for (const auto& s : schedulers()) sorted_scheds.push_back(s.get());
        std::ranges::sort(sorted_scheds, compare_perf);

        for (auto* sched : sorted_scheds) {
                if (sched->admission_test(new_task)) { return sched; }
        }

        return nullptr;
}
