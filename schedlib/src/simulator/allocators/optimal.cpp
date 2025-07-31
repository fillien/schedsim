#include "simulator/task.hpp"
#include <cstddef>
#include <print>
#include <simulator/allocator.hpp>
#include <simulator/allocators/optimal.hpp>
#include <simulator/scheduler.hpp>

#include <cstdio>
#include <memory>
#include <optional>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <stdexcept>
#include <vector>

auto allocators::Optimal::start() -> void
{

        if (exchange == nullptr) {
                tree = new Node;
        } else {
                tree = static_cast<Node*>(exchange);
        }
        current_root = tree;
}

auto allocators::Optimal::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() < second->cluster()->perf();
        };

        // Sort the schedulers by perf score
        auto sorted_scheds{schedulers()};

        std::optional<std::shared_ptr<scheds::Scheduler>> next_sched;

        if (current_root->children.empty()) {
                for (std::size_t i = 0; i < sorted_scheds.size() + 1; ++i) {
                        auto child = std::make_unique<Node>();
                        child->parent = current_root;
                        current_root->children.push_back(std::move(child));
                }
        }

        std::size_t choice = current_root->children.size();
        for (std::size_t i = 0; i < current_root->children.size(); ++i) {
                if (!current_root->children[i]->closed) {
                        choice = i;
                        break;
                }
        }

        pattern.push_back(choice);
        if (choice != 0) { next_sched = sorted_scheds.at(choice - 1); }

        current_root = current_root->children[choice].get();

        // Otherwise return that the allocator didn't found a cluster to place the task
        return next_sched;
}

auto allocators::Optimal::end() -> void
{
        const auto& last_choise = pattern.back();

        current_root->closed = true;

        while (current_root->parent != nullptr) {
                bool all_closed = true;
                for (auto & i : current_root->parent->children) {
                        if (!i->closed) { all_closed = false; }
                }
                current_root->parent->closed = all_closed;
                current_root = current_root->parent;
        }

        if (current_root->parent == nullptr) {
                bool all_closed = true;
                for (auto & i : current_root->children) {
                        if (!i->closed) { all_closed = false; }
                }

                // Check all allocation case have been explored
                if (all_closed) {
                        throw std::runtime_error("Finish");
                }
        }

        std::println("{0}", this->pattern);

        exchange = static_cast<void*>(tree);
}
