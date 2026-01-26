#include <simulator/allocators/counting.hpp>

#include <simulator/scheduler.hpp>
#include <simulator/task.hpp>

namespace allocators {

auto Counting::where_to_put_the_task(const Task& new_task) -> scheds::Scheduler*
{
        ++allocation_count_;
        for (const auto& scheduler : schedulers()) {
                if (scheduler->admission_test(new_task)) { return scheduler.get(); }
        }

        return nullptr;
}

} // namespace allocators
