#ifndef ALLOCATOR_COUNTING
#define ALLOCATOR_COUNTING

#include <cstddef>
#include <memory>
#include <optional>

#include <simulator/allocator.hpp>

namespace allocators {

/**
 * @brief Trivial allocator that only tracks how many placements it performs.
 *
 * @details Each time a task arrival requires a placement, the allocator picks
 * the first scheduler whose admission test succeeds. The primary metric is the
 * number of successful allocations, exposed via get_nb_alloc().
 */
class Counting : public Allocator {
      public:
        explicit Counting(const std::weak_ptr<Engine>& sim) : Allocator(sim) {}

        /**
         * @brief Number of successful allocations recorded so far.
         */
        auto get_nb_alloc() const -> std::size_t { return allocation_count_; }

      protected:
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> override;

      private:
        std::size_t allocation_count_ = 0;
};

} // namespace allocators

#endif // ALLOCATOR_COUNTING
