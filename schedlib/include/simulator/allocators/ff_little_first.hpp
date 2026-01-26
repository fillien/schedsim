#ifndef ALLOCATOR_FF_LITTLE_FIRST
#define ALLOCATOR_FF_LITTLE_FIRST

#include <cstddef>
#include <simulator/allocator.hpp>

namespace allocators {

class FFLittleFirst : public Allocator {
      protected:
        auto where_to_put_the_task(const Task& new_task) -> scheds::Scheduler* override;

      public:
        explicit FFLittleFirst(Engine& sim) : Allocator(sim) {};

        auto get_nb_alloc() const -> std::size_t { return step; };

      private:
        std::size_t step = 0;
};

}; // namespace allocators

#endif
