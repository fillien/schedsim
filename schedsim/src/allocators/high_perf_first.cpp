#include <allocator.hpp>
#include <allocators/high_perf_first.hpp>
#include <optional>

auto allocators::high_perf_first::where_to_put_the_task(const std::shared_ptr<task>& new_task)
    -> std::optional<std::shared_ptr<scheds::scheduler>>
{
        for (auto itr = std::begin(schedulers); itr != std::end(schedulers); ++itr) {
                if ((*itr)->admission_test(*new_task)) { return *itr; }
        }

        return {};
}
