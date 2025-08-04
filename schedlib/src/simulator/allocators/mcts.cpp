#include <optional>
#include <print>
#include <simulator/allocator.hpp>
#include <simulator/allocators/mcts.hpp>
#include <simulator/scheduler.hpp>
#include <random>

#include <vector>

template<typename T>
const T& pick_random(const std::vector<T>& vec) {
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<std::size_t> dist(0, vec.size() - 1);
    return vec[dist(gen)];
}

auto allocators::MCTS::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() < second->cluster()->perf();
        };

        // Sort the schedulers by perf score
        auto sorted_scheds{schedulers()};

        std::optional<std::shared_ptr<scheds::Scheduler>> next_sched;

        if (step < pattern.size()) {
                next_sched = sorted_scheds[pattern[step]];
        }
        // } else {
        //         next_sched = pick_random(sorted_scheds);
        // }
        step++;

        // Otherwise return that the allocator didn't found a cluster to place the task
        return next_sched;
}
