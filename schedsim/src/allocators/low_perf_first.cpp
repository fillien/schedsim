#include <algorithm>
#include <allocator.hpp>
#include <allocators/low_perf_first.hpp>
#include <optional>
#include <ranges>

auto allocators::LowPerfFirst::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() > second->cluster()->perf();
        };

        // Sort the schedulers by perf score
        auto sorted_scheds{schedulers()};
        std::ranges::sort(sorted_scheds, compare_perf);

        for (const auto& itr : std::ranges::reverse_view(sorted_scheds)) {
                if (itr->admission_test(*new_task)) { return itr; }
        }

        return {};
}
