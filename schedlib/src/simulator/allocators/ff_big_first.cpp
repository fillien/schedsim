#include "simulator/allocators/ff_big_first.hpp"
#include <algorithm>
#include <optional>
#include <simulator/allocator.hpp>

auto allocators::FFBigFirst::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() > second->cluster()->perf();
        };

        // Sort the schedulers by perf score
        auto sorted_scheds{schedulers()};
        std::ranges::sort(sorted_scheds, compare_perf);

        for (const auto& itr : sorted_scheds) {
                const auto& clu = itr->cluster();
                if (itr->admission_test(*new_task)) { return itr; }
        }

        return {};
}
