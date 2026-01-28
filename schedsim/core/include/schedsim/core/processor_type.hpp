#pragma once

#include <schedsim/core/types.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace schedsim::core {

// ProcessorType represents a category of processors with the same performance
// characteristics. Performance is a relative value; normalization to the
// reference (highest performance) is done by Platform.
class ProcessorType {
public:
    ProcessorType(std::size_t id, std::string_view name, double performance,
                  Duration context_switch_delay = Duration{0.0});

    [[nodiscard]] std::size_t id() const noexcept { return id_; }
    [[nodiscard]] std::string_view name() const noexcept { return name_; }
    [[nodiscard]] double performance() const noexcept { return performance_; }
    [[nodiscard]] Duration context_switch_delay() const noexcept { return context_switch_delay_; }

    // Non-copyable, movable (Decision 61)
    ProcessorType(const ProcessorType&) = delete;
    ProcessorType& operator=(const ProcessorType&) = delete;
    ProcessorType(ProcessorType&&) = default;
    ProcessorType& operator=(ProcessorType&&) = default;

private:
    std::size_t id_;
    std::string name_;
    double performance_;
    Duration context_switch_delay_;
};

} // namespace schedsim::core
