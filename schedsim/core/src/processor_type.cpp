#include <schedsim/core/processor_type.hpp>

namespace schedsim::core {

ProcessorType::ProcessorType(std::size_t id, std::string_view name, double performance,
                             Duration context_switch_delay)
    : id_(id)
    , name_(name)
    , performance_(performance)
    , context_switch_delay_(context_switch_delay) {}

} // namespace schedsim::core
