#ifndef ALLOCATOR_FF_CAP_ADAPTIVE_POLY
#define ALLOCATOR_FF_CAP_ADAPTIVE_POLY

#include <simulator/allocator.hpp>

namespace allocators {

class FFCapAdaptivePoly : public Allocator {
      private:
        double observed_umax_ = 0.0;
        double expected_total_util_ = 0.0;

      protected:
        auto where_to_put_the_task(const Task& new_task) -> scheds::Scheduler* override;

      public:
        explicit FFCapAdaptivePoly(Engine& sim) : Allocator(sim) {}

        void set_expected_total_util(double util) { expected_total_util_ = util; }
};

} // namespace allocators

#endif
