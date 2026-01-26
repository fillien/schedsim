#ifndef ALLOCATOR_FF_SMA
#define ALLOCATOR_FF_SMA

#include <simulator/allocator.hpp>

namespace allocators {

class FFSma : public Allocator {
      protected:
        auto where_to_put_the_task(const Task& new_task) -> scheds::Scheduler* override;

      public:
        explicit FFSma(Engine& sim, double sample_rate = 0.5, int num_samples = 5);

      private:
        double sample_rate_;
        int num_samples_;
};

}; // namespace allocators

#endif
