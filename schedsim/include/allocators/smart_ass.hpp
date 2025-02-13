#ifndef ALLOCATOR_SMART_ASS
#define ALLOCATOR_SMART_ASS

#include <allocator.hpp>
#include <optional>

namespace allocators {
class smart_ass : public allocator {
      protected:
        auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::scheduler>> final;

      public:
        explicit smart_ass(const std::weak_ptr<engine>& sim) : allocator(sim){};
};
}; // namespace allocators

#endif
