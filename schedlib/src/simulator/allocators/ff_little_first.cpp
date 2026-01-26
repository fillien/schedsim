#include <simulator/allocator.hpp>
#include <simulator/allocators/ff_little_first.hpp>

#include <algorithm>
#include <ranges>
#include <vector>

auto allocators::FFLittleFirst::where_to_put_the_task(const Task& new_task) -> scheds::Scheduler*
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() > second->cluster()->perf();
        };

        step++;
        // Sort the schedulers by perf score
        std::vector<scheds::Scheduler*> sorted_scheds;
        sorted_scheds.reserve(schedulers().size());
        for (const auto& s : schedulers()) sorted_scheds.push_back(s.get());
        std::ranges::sort(sorted_scheds, compare_perf);

        for (auto* sched : std::ranges::reverse_view(sorted_scheds)) {
                if (sched->admission_test(new_task)) { return sched; }
        }

        return nullptr;
}
