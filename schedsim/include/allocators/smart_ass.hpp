#ifndef ALLOCATOR_SMART_ASS
#define ALLOCATOR_SMART_ASS

#include <allocator.hpp>
#include <optional>

namespace allocators {
class SmartAss : public Allocator {
      protected:
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> final;

      public:
        explicit SmartAss(const std::weak_ptr<Engine>& sim) : Allocator(sim){};
};
}; // namespace allocators

#endif
