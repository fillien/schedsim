#include <allocator.hpp>
#include <allocators/high_perf_first.hpp>
#include <optional>

auto allocators::HighPerfFirst::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        for (const auto& itr : schedulers()) {
                if (itr->admission_test(*new_task)) { return itr; }
        }

        return {};
}
