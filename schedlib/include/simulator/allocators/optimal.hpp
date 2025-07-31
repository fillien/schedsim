#ifndef ALLOCATOR_OPTIMAL
#define ALLOCATOR_OPTIMAL

#include <cstddef>
#include <memory>
#include <optional>
#include <simulator/allocator.hpp>
#include <vector>
#include <rapidjson/document.h>

namespace allocators {

struct Node {
        Node* parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
        bool closed = false;

        // // Serialize Node into rapidjson Value (recursively)
        // void to_json(rapidjson::Value& value, rapidjson::Document::AllocatorType& allocator) const {
        //     value.SetObject();
        //     value.AddMember("closed", closed, allocator);
        //     rapidjson::Value childrenArray(rapidjson::kArrayType);
        //     for (const auto& child : children) {
        //         rapidjson::Value childValue;
        //         child->to_json(childValue, allocator);
        //         childrenArray.PushBack(childValue, allocator);
        //     }
        //     value.AddMember("children", childrenArray, allocator);
        // }

        // // Deserialize Node from rapidjson Value (recursively)
        // void from_json(const rapidjson::Value& value, Node* parent_ptr = nullptr) {
        //     parent = parent_ptr;
        //     closed = value["closed"].GetBool();
        //     children.clear();
        //     const auto& jsonChildren = value["children"];
        //     for (const auto& childVal : jsonChildren.GetArray()) {
        //             auto child = std::make_unique<Node>();
        //             child->from_json(childVal, this);
        //             children.push_back(std::move(child));
        //     }
        // }
};
/**
 * @brief A smart allocator class.
 *
 * This class inherits from the `Allocator` base class and provides a
 * mechanism for allocating tasks to schedulers based on some internal logic.
 */
class Optimal : public Allocator {
      protected:
        /**
         * @brief Determines where to place a new task.
         *
         * This virtual function is responsible for selecting the appropriate scheduler
         * for a given task. The implementation details are specific to this allocator.
         *
         * @param new_task A shared pointer to the task to be allocated.
         * @return An optional containing a shared pointer to the selected scheduler, or
         * std::nullopt if no suitable scheduler is found.
         */
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> override;

      public:
        /**
         * @brief Constructor for the SmartAss allocator.
         *
         * Initializes the `SmartAss` object with a weak pointer to the simulation engine.
         *
         * @param sim A weak pointer to the simulation engine.
         */
        explicit Optimal(const std::weak_ptr<Engine>& sim, void* exchange) : Allocator(sim), exchange(exchange) {};

        auto start() -> void override;
        auto end() -> void override;
        auto get_exchange() -> void* { return exchange; };

      private:
        void * exchange;
        Node* tree;
        Node* current_root {tree};
        std::vector<std::size_t> pattern;
};
}; // namespace allocators

#endif
