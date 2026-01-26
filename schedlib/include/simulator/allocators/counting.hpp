#ifndef ALLOCATOR_COUNTING
#define ALLOCATOR_COUNTING

#include <cstddef>
#include <simulator/allocator.hpp>

namespace allocators {

class Counting : public Allocator {
      public:
        explicit Counting(Engine& sim) : Allocator(sim) {}

        auto get_nb_alloc() const -> std::size_t { return allocation_count_; }

      protected:
        auto where_to_put_the_task(const Task& new_task) -> scheds::Scheduler* override;

      private:
        std::size_t allocation_count_ = 0;
};

} // namespace allocators

#endif // ALLOCATOR_COUNTING
