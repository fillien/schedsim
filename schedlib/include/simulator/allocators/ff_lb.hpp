#ifndef ALLOCATOR_FIRST_FIT_LOAD_BALANCER
#define ALLOCATOR_FIRST_FIT_LOAD_BALANCER

#include <simulator/allocator.hpp>

namespace allocators {

class FirstFitLoadBalancer : public Allocator {
      protected:
        auto where_to_put_the_task(const Task& new_task) -> scheds::Scheduler* override;

      public:
        explicit FirstFitLoadBalancer(Engine& sim) : Allocator(sim) {};
};

}; // namespace allocators

#endif
