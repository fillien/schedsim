#ifndef ALLOCATOR_SMART_ASS
#define ALLOCATOR_SMART_ASS

#include <meta_scheduler.hpp>

namespace allocators {
class smart_ass : public meta_scheduler {
      protected:
        auto where_to_put_the_task(const std::shared_ptr<task>& new_task)
            -> std::pair<std::shared_ptr<scheduler>, bool> final;

      public:
        explicit smart_ass(const std::weak_ptr<engine> sim) : meta_scheduler(sim){};
};
}; // namespace allocators

#endif
