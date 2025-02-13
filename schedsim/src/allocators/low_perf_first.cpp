#include <allocator.hpp>
#include <allocators/low_perf_first.hpp>
#include <iterator>
#include <optional>

auto allocators::LowPerfFirst::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        for (auto itr = std::rbegin(schedulers); itr != std::rend(schedulers); ++itr) {
                if ((*itr)->admission_test(*new_task)) { return *itr; }
        }

        return {};
}
