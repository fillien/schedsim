#include <allocator.hpp>
#include <allocators/low_perf_first.hpp>
#include <optional>
#include <ranges>

auto allocators::LowPerfFirst::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        for (const auto& itr : std::ranges::reverse_view(schedulers())) {
                if (itr->admission_test(*new_task)) { return itr; }
        }

        return {};
}
