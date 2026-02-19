#pragma once

#include <schedsim/core/types.hpp>

#include <cstdint>
#include <string_view>

namespace schedsim::core {

/// @brief Abstract interface for recording simulation trace events.
/// @ingroup core
///
/// Implementations of TraceWriter serialise simulation events to a
/// specific format (JSON, binary, socket, memory buffer, etc.).
/// Each trace record is built incrementally:
///   1. begin() -- opens a new record at a given simulation time
///   2. type()  -- sets the event type name
///   3. field() -- (repeated) adds key/value data fields
///   4. end()   -- closes and optionally flushes the record
///
/// The Engine holds an optional pointer to a TraceWriter. When no writer
/// is installed the overhead is a single null-pointer check (zero-cost).
///
/// @see Engine::set_trace_writer()
class TraceWriter {
public:
    /// @brief Virtual destructor for safe polymorphic deletion.
    virtual ~TraceWriter() = default;

    /// @brief Begin a new trace record at the given simulation time.
    /// @param time The simulation time-point at which the event occurs.
    virtual void begin(TimePoint time) = 0;

    /// @brief Set the event type name for the current record.
    /// @param name A short identifier for the event category
    ///        (e.g. `"dispatch"`, `"preempt"`, `"freq_change"`).
    virtual void type(std::string_view name) = 0;

    /// @brief Add a floating-point field to the current record.
    /// @param key   Field name.
    /// @param value Field value.
    virtual void field(std::string_view key, double value) = 0;

    /// @brief Add an unsigned integer field to the current record.
    /// @param key   Field name.
    /// @param value Field value.
    virtual void field(std::string_view key, uint64_t value) = 0;

    /// @brief Add a string field to the current record.
    /// @param key   Field name.
    /// @param value Field value.
    virtual void field(std::string_view key, std::string_view value) = 0;

    /// @brief End the current record and flush if needed.
    ///
    /// After this call the writer is ready for a new begin()/end() cycle.
    virtual void end() = 0;

protected:
    /// @brief Default constructor (protected -- instantiate subclasses only).
    TraceWriter() = default;

    /// @brief Copy constructor (protected).
    TraceWriter(const TraceWriter&) = default;

    /// @brief Copy-assignment operator (protected).
    /// @return Reference to this writer.
    TraceWriter& operator=(const TraceWriter&) = default;

    /// @brief Move constructor (protected).
    TraceWriter(TraceWriter&&) = default;

    /// @brief Move-assignment operator (protected).
    /// @return Reference to this writer.
    TraceWriter& operator=(TraceWriter&&) = default;
};

} // namespace schedsim::core
