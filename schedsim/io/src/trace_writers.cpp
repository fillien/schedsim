#include <schedsim/io/trace_writers.hpp>

#include <iomanip>
#include <sstream>

namespace schedsim::io {

// =============================================================================
// NullTraceWriter
// =============================================================================

void NullTraceWriter::begin(core::TimePoint /*time*/) {}
void NullTraceWriter::type(std::string_view /*name*/) {}
void NullTraceWriter::field(std::string_view /*key*/, double /*value*/) {}
void NullTraceWriter::field(std::string_view /*key*/, uint64_t /*value*/) {}
void NullTraceWriter::field(std::string_view /*key*/, std::string_view /*value*/) {}
void NullTraceWriter::end() {}

// =============================================================================
// JsonTraceWriter
// =============================================================================

JsonTraceWriter::JsonTraceWriter(std::ostream& output)
    : output_(output) {
    output_ << "[\n";
}

JsonTraceWriter::~JsonTraceWriter() {
    if (!finalized_) {
        finalize();
    }
}

void JsonTraceWriter::begin(core::TimePoint time) {
    if (!first_record_) {
        output_ << ",\n";
    }
    first_record_ = false;
    in_record_ = true;
    first_field_ = true;

    output_ << "  {\"time\": " << std::setprecision(15) << time.time_since_epoch().count();
}

void JsonTraceWriter::type(std::string_view name) {
    output_ << ", \"type\": \"" << name << "\"";
}

void JsonTraceWriter::write_field_separator() {
    output_ << ", ";
}

std::string JsonTraceWriter::escape_json_string(std::string_view str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u" << std::hex << std::setfill('0')
                        << std::setw(4) << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

void JsonTraceWriter::field(std::string_view key, double value) {
    write_field_separator();
    output_ << "\"" << key << "\": " << std::setprecision(15) << value;
}

void JsonTraceWriter::field(std::string_view key, uint64_t value) {
    write_field_separator();
    output_ << "\"" << key << "\": " << value;
}

void JsonTraceWriter::field(std::string_view key, std::string_view value) {
    write_field_separator();
    output_ << "\"" << key << "\": \"" << escape_json_string(value) << "\"";
}

void JsonTraceWriter::end() {
    output_ << "}";
    in_record_ = false;
}

void JsonTraceWriter::finalize() {
    if (!finalized_) {
        if (!first_record_) {
            // Records were written, add newline before closing bracket
            output_ << "\n";
        }
        output_ << "]\n";
        output_.flush();
        finalized_ = true;
    }
}

// =============================================================================
// MemoryTraceWriter
// =============================================================================

void MemoryTraceWriter::begin(core::TimePoint time) {
    current_ = TraceRecord{};
    current_.time = time.time_since_epoch().count();
}

void MemoryTraceWriter::type(std::string_view name) {
    current_.type = std::string(name);
}

void MemoryTraceWriter::field(std::string_view key, double value) {
    current_.fields[std::string(key)] = value;
}

void MemoryTraceWriter::field(std::string_view key, uint64_t value) {
    current_.fields[std::string(key)] = value;
}

void MemoryTraceWriter::field(std::string_view key, std::string_view value) {
    current_.fields[std::string(key)] = std::string(value);
}

void MemoryTraceWriter::end() {
    records_.push_back(std::move(current_));
    current_ = TraceRecord{};
}

} // namespace schedsim::io
