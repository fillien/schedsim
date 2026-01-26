#ifndef ALLOCATOR_FF_CAP
#define ALLOCATOR_FF_CAP

#include <simulator/allocator.hpp>

namespace allocators {

class FFCap : public Allocator {
      protected:
        auto where_to_put_the_task(const Task& new_task) -> scheds::Scheduler* override;

      public:
        explicit FFCap(Engine& sim) : Allocator(sim) {};
};

}; // namespace allocators

#endif
