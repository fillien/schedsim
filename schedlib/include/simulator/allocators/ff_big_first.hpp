#ifndef ALLOCATOR_FF_BIG_FIRST
#define ALLOCATOR_FF_BIG_FIRST

#include <simulator/allocator.hpp>

namespace allocators {

class FFBigFirst : public Allocator {
      protected:
        auto where_to_put_the_task(const Task& new_task) -> scheds::Scheduler* override;

      public:
        explicit FFBigFirst(Engine& sim) : Allocator(sim) {};
};

} // namespace allocators

#endif
