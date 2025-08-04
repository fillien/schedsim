#ifndef ALLOCATOR_MCTS
#define ALLOCATOR_MCTS

#include <cstddef>
#include <memory>
#include <optional>
#include <simulator/allocator.hpp>
#include <vector>
#include <rapidjson/document.h>

namespace allocators {

/**
 * @brief A smart allocator class.
 *
 * This class inherits from the `Allocator` base class and provides a
 * mechanism for allocating tasks to schedulers based on some internal logic.
 */
class MCTS : public Allocator {
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
        explicit MCTS(const std::weak_ptr<Engine>& sim, const std::vector<unsigned>& pattern) : Allocator(sim), pattern(pattern) {};

        auto get_nb_alloc() const -> std::size_t { return step; };

       private:
        std::vector<unsigned> pattern;
        std::size_t step = 0;
};
}; // namespace allocators

#endif
