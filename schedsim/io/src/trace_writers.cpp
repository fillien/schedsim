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

// =============================================================================
// TextualTraceWriter
// =============================================================================

TextualTraceWriter::TextualTraceWriter(std::ostream& output, bool color_enabled)
    : output_(output)
    , color_enabled_(color_enabled) {}

void TextualTraceWriter::begin(core::TimePoint time) {
    current_time_ = time.time_since_epoch().count();
    current_type_.clear();
    current_fields_.clear();
}

void TextualTraceWriter::type(std::string_view name) {
    current_type_ = std::string(name);
}

void TextualTraceWriter::field(std::string_view key, double value) {
    std::ostringstream oss;
    oss << std::setprecision(10) << value;
    current_fields_.push_back({std::string(key), oss.str()});
}

void TextualTraceWriter::field(std::string_view key, uint64_t value) {
    current_fields_.push_back({std::string(key), std::to_string(value)});
}

void TextualTraceWriter::field(std::string_view key, std::string_view value) {
    current_fields_.push_back({std::string(key), std::string(value)});
}

void TextualTraceWriter::end() {
    // Format: [  timestamp] (+  delta)   event_name: key = value, key = value
    output_ << "[" << std::setw(10) << std::fixed << std::setprecision(5)
            << current_time_ << "] ";

    // Delta from previous event
    if (prev_time_ >= 0.0 && current_time_ != prev_time_) {
        output_ << "(+" << std::setw(10) << std::fixed << std::setprecision(5)
                << (current_time_ - prev_time_) << ") ";
    } else {
        output_ << "(           ) ";
    }

    // Event name (right-aligned in 30 chars)
    output_ << std::setw(30) << std::right << current_type_ << ":";

    // Fields
    for (std::size_t i = 0; i < current_fields_.size(); ++i) {
        if (i > 0) {
            output_ << ",";
        }
        output_ << " " << current_fields_[i].key << " = " << current_fields_[i].value;
    }

    output_ << "\n";
    prev_time_ = current_time_;
}

} // namespace schedsim::io
