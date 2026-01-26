#include <print>
#include <random>
#include <simulator/allocator.hpp>
#include <simulator/allocators/mcts.hpp>
#include <simulator/scheduler.hpp>

#include <vector>

template <typename T> const T& pick_random(const std::vector<T>& vec)
{
        auto next_rand = []() -> unsigned long long {
                static thread_local unsigned long long x = 0x9E3779B97F4A7C15ull;
                x ^= x >> 12;
                x ^= x << 25;
                x ^= x >> 27;
                return x * 2685821657736338717ull;
        };

        const auto n = vec.size();

        if (n == 2) {
                const auto bit = static_cast<std::size_t>(next_rand() & 1ull);
                return vec[bit];
        }

        return vec[static_cast<std::size_t>(next_rand() % n)];
}

auto allocators::MCTS::where_to_put_the_task(const Task& new_task) -> scheds::Scheduler*
{
        // Build raw-pointer vector from unique_ptr schedulers
        std::vector<scheds::Scheduler*> sorted_scheds;
        sorted_scheds.reserve(schedulers().size());
        for (const auto& s : schedulers()) sorted_scheds.push_back(s.get());

        scheds::Scheduler* next_sched = nullptr;

        if (step < pattern.size()) { next_sched = sorted_scheds[pattern[step]]; }
        else {
                next_sched = pick_random(sorted_scheds);
        }
        step++;

        // Otherwise return that the allocator didn't find a cluster to place the task
        return next_sched;
}
