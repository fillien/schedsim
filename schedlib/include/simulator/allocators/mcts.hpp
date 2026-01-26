#ifndef ALLOCATOR_MCTS
#define ALLOCATOR_MCTS

#include <cstddef>
#include <simulator/allocator.hpp>
#include <vector>

namespace allocators {

class MCTS : public Allocator {
      protected:
        auto where_to_put_the_task(const Task& new_task) -> scheds::Scheduler* override;

      public:
        explicit MCTS(Engine& sim, const std::vector<unsigned>& pattern)
            : Allocator(sim), pattern(pattern) {};

        auto get_nb_alloc() const -> std::size_t { return step; };

      private:
        std::vector<unsigned> pattern;
        std::size_t step = 0;
};

}; // namespace allocators

#endif
