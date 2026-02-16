#pragma once

#include <schedsim/core/trace_writer.hpp>

#include <ostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace schedsim::io {

// Discards all traces (maximum performance)
class NullTraceWriter : public core::TraceWriter {
public:
    void begin(core::TimePoint time) override;
    void type(std::string_view name) override;
    void field(std::string_view key, double value) override;
    void field(std::string_view key, uint64_t value) override;
    void field(std::string_view key, std::string_view value) override;
    void end() override;
};

// Streams JSON to output stream (file or stdout)
// Non-copyable, non-movable (holds reference to output stream)
class JsonTraceWriter : public core::TraceWriter {
public:
    explicit JsonTraceWriter(std::ostream& output);
    ~JsonTraceWriter() override;

    JsonTraceWriter(const JsonTraceWriter&) = delete;
    JsonTraceWriter& operator=(const JsonTraceWriter&) = delete;
    JsonTraceWriter(JsonTraceWriter&&) = delete;
    JsonTraceWriter& operator=(JsonTraceWriter&&) = delete;

    void begin(core::TimePoint time) override;
    void type(std::string_view name) override;
    void field(std::string_view key, double value) override;
    void field(std::string_view key, uint64_t value) override;
    void field(std::string_view key, std::string_view value) override;
    void end() override;

    void finalize();  // Write closing bracket

private:
    void write_field_separator();
    static std::string escape_json_string(std::string_view str);

    std::ostream& output_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    bool first_record_{true};
    bool in_record_{false};
    bool first_field_{true};
    bool finalized_{false};
};

// Structured trace record for in-memory storage
struct TraceRecord {
    double time;
    std::string type;
    std::unordered_map<std::string, std::variant<double, uint64_t, std::string>> fields;
};

// Buffers traces in memory for post-processing
class MemoryTraceWriter : public core::TraceWriter {
public:
    void begin(core::TimePoint time) override;
    void type(std::string_view name) override;
    void field(std::string_view key, double value) override;
    void field(std::string_view key, uint64_t value) override;
    void field(std::string_view key, std::string_view value) override;
    void end() override;

    [[nodiscard]] const std::vector<TraceRecord>& records() const { return records_; }
    void clear() { records_.clear(); }

private:
    std::vector<TraceRecord> records_;
    TraceRecord current_;
};

// Colored human-readable textual trace output (matches legacy format)
class TextualTraceWriter : public core::TraceWriter {
public:
    explicit TextualTraceWriter(std::ostream& output, bool color_enabled = true);

    TextualTraceWriter(const TextualTraceWriter&) = delete;
    TextualTraceWriter& operator=(const TextualTraceWriter&) = delete;
    TextualTraceWriter(TextualTraceWriter&&) = delete;
    TextualTraceWriter& operator=(TextualTraceWriter&&) = delete;

    void begin(core::TimePoint time) override;
    void type(std::string_view name) override;
    void field(std::string_view key, double value) override;
    void field(std::string_view key, uint64_t value) override;
    void field(std::string_view key, std::string_view value) override;
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
