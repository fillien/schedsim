#pragma once

/// @file trace_writers.hpp
/// @brief Concrete TraceWriter implementations for simulation output.
///
/// Provides several writers that implement the @ref core::TraceWriter
/// interface: a no-op writer for benchmarking, a JSON streaming writer,
/// an in-memory buffer for post-processing, and a human-readable textual
/// writer with optional ANSI colour output.
///
/// @ingroup io_writers

#include <schedsim/core/trace_writer.hpp>

#include <ostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace schedsim::io {

/// @brief Trace writer that silently discards all events.
///
/// Useful when trace output is not needed and maximum simulation
/// performance is desired (zero overhead per event).
///
/// @ingroup io_writers
/// @see core::TraceWriter
class NullTraceWriter : public core::TraceWriter {
public:
    /// @brief Begin a new trace record at the given simulation time.
    /// @param time  Current simulation time.
    void begin(core::TimePoint time) override;

    /// @brief Set the event type name (ignored).
    /// @param name  Event type identifier.
    void type(std::string_view name) override;

    /// @brief Record a floating-point field (ignored).
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, double value) override;

    /// @brief Record an unsigned integer field (ignored).
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, uint64_t value) override;

    /// @brief Record a string field (ignored).
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, std::string_view value) override;

    /// @brief Finalise the current record (no-op).
    void end() override;
};

/// @brief Trace writer that streams JSON array elements to an output stream.
///
/// Writes one JSON object per trace event directly to the provided stream
/// (file or stdout). Call @ref finalize to emit the closing bracket once
/// the simulation is complete.
///
/// Non-copyable and non-movable because it holds a reference to the
/// output stream.
///
/// @ingroup io_writers
/// @see core::TraceWriter, MemoryTraceWriter, TextualTraceWriter
class JsonTraceWriter : public core::TraceWriter {
public:
    /// @brief Construct a JSON writer targeting @p output.
    /// @param output  Destination stream (must outlive this writer).
    explicit JsonTraceWriter(std::ostream& output);

    /// @brief Destructor; calls @ref finalize if not already called.
    ~JsonTraceWriter() override;

    JsonTraceWriter(const JsonTraceWriter&) = delete;
    JsonTraceWriter& operator=(const JsonTraceWriter&) = delete;
    JsonTraceWriter(JsonTraceWriter&&) = delete;
    JsonTraceWriter& operator=(JsonTraceWriter&&) = delete;

    /// @brief Begin a new JSON trace record.
    /// @param time  Current simulation time.
    void begin(core::TimePoint time) override;

    /// @brief Set the event type field in the current JSON object.
    /// @param name  Event type identifier.
    void type(std::string_view name) override;

    /// @brief Add a floating-point field to the current JSON object.
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, double value) override;

    /// @brief Add an unsigned integer field to the current JSON object.
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, uint64_t value) override;

    /// @brief Add a string field to the current JSON object.
    /// @param key    Field name.
    /// @param value  Field value (will be JSON-escaped).
    void field(std::string_view key, std::string_view value) override;

    /// @brief Close the current JSON object.
    void end() override;

    /// @brief Write the closing bracket of the JSON array.
    ///
    /// Must be called exactly once after all events have been written.
    /// The destructor calls this automatically if it has not been invoked.
    void finalize();

private:
    void write_field_separator();
    static std::string escape_json_string(std::string_view str);

    std::ostream& output_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    bool first_record_{true};
    bool in_record_{false};
    bool first_field_{true};
    bool finalized_{false};
};

/// @brief A single trace record stored in memory.
///
/// Each record captures the simulation time, event type, and an arbitrary
/// set of named fields whose values may be @c double, @c uint64_t, or
/// @c std::string.
///
/// @ingroup io_writers
/// @see MemoryTraceWriter, compute_metrics
struct TraceRecord {
    double time;        ///< Simulation time of the event (seconds).
    std::string type;   ///< Event type identifier (e.g. "job_completed").
    /// @brief Named fields attached to the event.
    ///
    /// Keys are field names; values are one of @c double, @c uint64_t,
    /// or @c std::string.
    std::unordered_map<std::string, std::variant<double, uint64_t, std::string>> fields;
};

/// @brief Trace writer that buffers all events in memory as @ref TraceRecord objects.
///
/// Ideal for unit tests and post-simulation analysis where the full trace
/// must be inspected programmatically.
///
/// @ingroup io_writers
/// @see TraceRecord, compute_metrics, JsonTraceWriter
class MemoryTraceWriter : public core::TraceWriter {
public:
    /// @brief Begin a new in-memory trace record.
    /// @param time  Current simulation time.
    void begin(core::TimePoint time) override;

    /// @brief Set the event type on the current record.
    /// @param name  Event type identifier.
    void type(std::string_view name) override;

    /// @brief Attach a floating-point field to the current record.
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, double value) override;

    /// @brief Attach an unsigned integer field to the current record.
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, uint64_t value) override;

    /// @brief Attach a string field to the current record.
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, std::string_view value) override;

    /// @brief Finalise the current record and append it to the buffer.
    void end() override;

    /// @brief Access the accumulated trace records.
    /// @return Const reference to the internal record vector.
    [[nodiscard]] const std::vector<TraceRecord>& records() const { return records_; }

    /// @brief Discard all buffered records.
    void clear() { records_.clear(); }

private:
    std::vector<TraceRecord> records_;
    TraceRecord current_;
};

/// @brief Human-readable textual trace writer with optional ANSI colour.
///
/// Formats each event as a single line with aligned columns, matching the
/// legacy simulator's output style. Colour can be disabled for piping to
/// files or non-terminal sinks.
///
/// Non-copyable and non-movable because it holds a reference to the
/// output stream.
///
/// @ingroup io_writers
/// @see core::TraceWriter, JsonTraceWriter, NullTraceWriter
class TextualTraceWriter : public core::TraceWriter {
public:
    /// @brief Construct a textual writer targeting @p output.
    /// @param output         Destination stream (must outlive this writer).
    /// @param color_enabled  If true, emit ANSI escape codes for colour.
    explicit TextualTraceWriter(std::ostream& output, bool color_enabled = true);

    TextualTraceWriter(const TextualTraceWriter&) = delete;
    TextualTraceWriter& operator=(const TextualTraceWriter&) = delete;
    TextualTraceWriter(TextualTraceWriter&&) = delete;
    TextualTraceWriter& operator=(TextualTraceWriter&&) = delete;

    /// @brief Begin a new textual trace line.
    /// @param time  Current simulation time.
    void begin(core::TimePoint time) override;

    /// @brief Set the event type for the current line.
    /// @param name  Event type identifier.
    void type(std::string_view name) override;

    /// @brief Buffer a floating-point field for the current line.
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, double value) override;

    /// @brief Buffer an unsigned integer field for the current line.
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, uint64_t value) override;

    /// @brief Buffer a string field for the current line.
    /// @param key    Field name.
    /// @param value  Field value.
    void field(std::string_view key, std::string_view value) override;

    /// @brief Flush the buffered fields and write the formatted line.
    void end() override;

private:
    struct FieldEntry {
        std::string key;
        std::string value;
    };

    std::ostream& output_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    bool color_enabled_;
    double current_time_{0.0};
    double prev_time_{-1.0};
    std::string current_type_;
    std::vector<FieldEntry> current_fields_;
};

} // namespace schedsim::io
