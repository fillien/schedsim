#ifndef FACTORY_HPP
#define FACTORY_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Engine;

namespace allocators {
class Allocator;
}

namespace scheds {
class Scheduler;
}

auto parse_allocator_args(const std::vector<std::string>& raw_args)
    -> std::unordered_map<std::string, std::string>;

auto select_alloc(
    const std::string& choice,
    Engine& sim,
    const std::unordered_map<std::string, std::string>& alloc_args)
    -> std::unique_ptr<allocators::Allocator>;

auto select_sched(const std::string& choice, Engine& sim)
    -> std::unique_ptr<scheds::Scheduler>;

#endif // FACTORY_HPP
