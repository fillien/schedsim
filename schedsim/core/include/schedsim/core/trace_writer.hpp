#pragma once

#include <schedsim/core/types.hpp>

#include <cstdint>
#include <string_view>

namespace schedsim::core {

// TraceWriter interface for recording simulation events (Decision 35, 80)
// Implementations may write to JSON, binary, socket, or other formats
class TraceWriter {
public:
    virtual ~TraceWriter() = default;

    // Begin a new trace record at the given simulation time
    virtual void begin(TimePoint time) = 0;

    // Set the event type name
    virtual void type(std::string_view name) = 0;

    // Add fields to the current record
    virtual void field(std::string_view key, double value) = 0;
    virtual void field(std::string_view key, uint64_t value) = 0;
    virtual void field(std::string_view key, std::string_view value) = 0;

    // End the current record and flush if needed
    virtual void end() = 0;

protected:
    TraceWriter() = default;
    TraceWriter(const TraceWriter&) = default;
    TraceWriter& operator=(const TraceWriter&) = default;
    TraceWriter(TraceWriter&&) = default;
    TraceWriter& operator=(TraceWriter&&) = default;
};

} // namespace schedsim::core
