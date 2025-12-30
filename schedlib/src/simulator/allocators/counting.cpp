#include <simulator/allocators/counting.hpp>

#include <optional>

#include <simulator/scheduler.hpp>
#include <simulator/task.hpp>

namespace allocators {

auto Counting::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        ++allocation_count_;
        for (const auto& scheduler : schedulers()) {
                if (scheduler->admission_test(*new_task)) {
                        return scheduler;
                }
        }

        return std::nullopt;
}

} // namespace allocators
