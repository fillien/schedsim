#ifndef ALLOCATOR_FF_U_CAP_FITTED
#define ALLOCATOR_FF_U_CAP_FITTED

#include <simulator/allocator.hpp>

namespace allocators {

class FFUCapFitted : public Allocator {
      protected:
        auto where_to_put_the_task(const Task& new_task) -> scheds::Scheduler* override;

      public:
        explicit FFUCapFitted(Engine& sim) : Allocator(sim) {}
};

} // namespace allocators

#endif
