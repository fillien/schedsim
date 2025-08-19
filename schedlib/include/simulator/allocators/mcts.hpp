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
 * @brief Deterministic pattern-based allocator for experiments.
 *
 * @details Instead of a search, this allocator selects the next scheduler
 * according to a provided index pattern (one index per allocation step).
 * Useful to reproduce specific allocation sequences.
 */
class MCTS : public Allocator {
      protected:
        /**
         * @brief Pick the scheduler at the current pattern index.
         *
         * @param new_task Task to be scheduled (unused for selection).
         * @return Selected scheduler if within pattern bounds, otherwise std::nullopt.
         */
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> override;

      public:
        /**
         * @brief Construct the allocator with a fixed selection pattern.
         *
         * @param sim Weak pointer to the simulation engine.
         * @param pattern Vector of indices into the scheduler list; the i-th
         * element selects the scheduler for the i-th allocation.
         */
        explicit MCTS(const std::weak_ptr<Engine>& sim, const std::vector<unsigned>& pattern) : Allocator(sim), pattern(pattern) {};

        /**
         * @brief Number of allocations performed so far.
         * @return Count of allocation decisions made.
         */
        auto get_nb_alloc() const -> std::size_t { return step; };

       private:
        std::vector<unsigned> pattern;
        std::size_t step = 0;
};
}; // namespace allocators

#endif
