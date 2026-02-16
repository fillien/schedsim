#include <schedsim/core/task.hpp>
#include <schedsim/core/processor_type.hpp>

namespace schedsim::core {

Task::Task(std::size_t id, Duration period, Duration relative_deadline, Duration wcet)
    : id_(id)
    , period_(period)
    , relative_deadline_(relative_deadline)
    , wcet_(wcet) {}

Duration Task::wcet(const ProcessorType& type, double reference_performance) const noexcept {
    // Wall-clock time = reference_wcet / (type_performance / reference_performance)
    // = reference_wcet * reference_performance / type_performance
    double normalized_speed = type.performance() / reference_performance;
    return divide_duration(wcet_, normalized_speed);
}

} // namespace schedsim::core
