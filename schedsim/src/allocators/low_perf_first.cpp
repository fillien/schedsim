#include <allocator.hpp>
#include <allocators/low_perf_first.hpp>
#include <iterator>
#include <allocator.hpp>

auto allocators::low_perf_first::where_to_put_the_task(const std::shared_ptr<task>& new_task)
    -> std::pair<std::shared_ptr<scheds::scheduler>, bool>
{
        for (auto itr = std::rbegin(schedulers); itr != std::rend(schedulers); ++itr) {
                if ((*itr)->admission_test(*new_task)) { return {*itr, true}; }
        }

        return {schedulers.at(0), false};
}
