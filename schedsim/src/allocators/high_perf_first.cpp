#include <allocators/high_perf_first.hpp>
#include <allocator.hpp>

auto allocators::high_perf_first::where_to_put_the_task(const std::shared_ptr<task>& new_task)
    -> std::pair<std::shared_ptr<scheds::scheduler>, bool>
{
        for (auto itr = std::begin(schedulers); itr != std::end(schedulers); ++itr) {
                if ((*itr)->admission_test(*new_task)) { return {*itr, true}; }
        }

        return {schedulers.at(0), false};
}
