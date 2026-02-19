#pragma once

#include <schedsim/core/types.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace schedsim::core {

/// @brief Represents a category of processors sharing the same performance characteristics.
/// @ingroup core_hardware
///
/// Performance is a relative value expressing how fast processors of this type
/// execute work compared to other types. Normalization to the reference
/// (highest-performance) type is performed by Platform.
///
/// ProcessorTypes are non-copyable but movable, following Decision 61.
///
/// @see Processor, Platform, Task::wcet(const ProcessorType&, double)
class ProcessorType {
public:
    /// @brief Construct a new ProcessorType.
    /// @param id                   Unique identifier for this processor type.
    /// @param name                 Human-readable name (e.g., "A15", "A7").
    /// @param performance          Relative performance factor (higher is faster).
    /// @param context_switch_delay Overhead duration for a context switch on this type.
    ///                             Defaults to zero.
    ProcessorType(std::size_t id, std::string_view name, double performance,
                  Duration context_switch_delay = duration_from_seconds(0.0));

    /// @brief Get the unique processor type identifier.
    /// @return Processor type ID.
    [[nodiscard]] std::size_t id() const noexcept { return id_; }

    /// @brief Get the human-readable name of this processor type.
    /// @return Name as a string_view.
    [[nodiscard]] std::string_view name() const noexcept { return name_; }

    /// @brief Get the relative performance factor.
    /// @return Performance value (higher means faster execution).
    /// @see Task::wcet(const ProcessorType&, double)
    [[nodiscard]] double performance() const noexcept { return performance_; }

    /// @brief Get the context switch delay for processors of this type.
    /// @return Context switch overhead as a Duration.
    /// @see Processor
    [[nodiscard]] Duration context_switch_delay() const noexcept { return context_switch_delay_; }

    // Non-copyable, movable (Decision 61)
    ProcessorType(const ProcessorType&) = delete;
    ProcessorType& operator=(const ProcessorType&) = delete;
    /// @cond INTERNAL
    ProcessorType(ProcessorType&&) = default;
    ProcessorType& operator=(ProcessorType&&) = default;
    /// @endcond

private:
    std::size_t id_;
    std::string name_;
    double performance_;
    Duration context_switch_delay_;
};

} // namespace schedsim::core
