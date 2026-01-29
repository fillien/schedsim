#include <schedsim/io/trace_writers.hpp>

#include <gtest/gtest.h>

#include <sstream>

using namespace schedsim::io;
using namespace schedsim::core;

class TraceWritersTest : public ::testing::Test {
protected:
    TimePoint time(double seconds) {
        return TimePoint{Duration{seconds}};
    }
};

// =============================================================================
// NullTraceWriter Tests
// =============================================================================

TEST_F(TraceWritersTest, NullWriterAcceptsAllCalls) {
    NullTraceWriter writer;

    // Should not throw or crash
    writer.begin(time(0.0));
    writer.type("test_event");
    writer.field("int_field", uint64_t{42});
    writer.field("double_field", 3.14);
    writer.field("string_field", "hello");
    writer.end();

    writer.begin(time(1.0));
    writer.type("another_event");
    writer.end();
}

// =============================================================================
// JsonTraceWriter Tests
// =============================================================================

TEST_F(TraceWritersTest, JsonWriterProducesValidArray) {
    std::ostringstream oss;
    {
        JsonTraceWriter writer(oss);
        // Let destructor call finalize
    }

    std::string output = oss.str();
    EXPECT_EQ(output, "[\n]\n");
}

TEST_F(TraceWritersTest, JsonWriterSingleRecord) {
    std::ostringstream oss;
    {
        JsonTraceWriter writer(oss);
        writer.begin(time(1.5));
        writer.type("test_event");
        writer.field("count", uint64_t{10});
        writer.end();
    }

    std::string output = oss.str();
    EXPECT_NE(output.find("\"time\": 1.5"), std::string::npos);
    EXPECT_NE(output.find("\"type\": \"test_event\""), std::string::npos);
    EXPECT_NE(output.find("\"count\": 10"), std::string::npos);
    // Should be valid JSON array
    EXPECT_EQ(output.front(), '[');
    EXPECT_NE(output.find("]\n"), std::string::npos);
}

TEST_F(TraceWritersTest, JsonWriterMultipleRecords) {
    std::ostringstream oss;
    {
        JsonTraceWriter writer(oss);
        writer.begin(time(0.0));
        writer.type("event1");
        writer.end();

        writer.begin(time(1.0));
        writer.type("event2");
        writer.end();

        writer.begin(time(2.0));
        writer.type("event3");
        writer.end();
    }

    std::string output = oss.str();
    // Count occurrences of "type"
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = output.find("\"type\"", pos)) != std::string::npos) {
        ++count;
        ++pos;
    }
    EXPECT_EQ(count, 3u);
}

TEST_F(TraceWritersTest, JsonWriterFinalizeCanBeCalledMultipleTimes) {
    std::ostringstream oss;
    JsonTraceWriter writer(oss);
    writer.begin(time(0.0));
    writer.type("test");
    writer.end();

    writer.finalize();
    writer.finalize();  // Should be no-op

    std::string output = oss.str();
    // Should only have one closing bracket
    std::size_t bracket_count = 0;
    for (char ch : output) {
        if (ch == ']') {
            ++bracket_count;
        }
    }
    EXPECT_EQ(bracket_count, 1u);
}

TEST_F(TraceWritersTest, JsonWriterEscapesStrings) {
    std::ostringstream oss;
    {
        JsonTraceWriter writer(oss);
        writer.begin(time(0.0));
        writer.type("test");
        writer.field("message", "hello \"world\" with\\backslash");
        writer.end();
    }

    std::string output = oss.str();
    EXPECT_NE(output.find("\\\"world\\\""), std::string::npos);
    EXPECT_NE(output.find("\\\\backslash"), std::string::npos);
}

TEST_F(TraceWritersTest, JsonWriterAllFieldTypes) {
    std::ostringstream oss;
    {
        JsonTraceWriter writer(oss);
        writer.begin(time(0.0));
        writer.type("test");
        writer.field("uint_field", uint64_t{123456789});
        writer.field("double_field", 3.14159);
        writer.field("string_field", "test_value");
        writer.end();
    }

    std::string output = oss.str();
    EXPECT_NE(output.find("\"uint_field\": 123456789"), std::string::npos);
    EXPECT_NE(output.find("\"double_field\":"), std::string::npos);
    EXPECT_NE(output.find("\"string_field\": \"test_value\""), std::string::npos);
}

// =============================================================================
// MemoryTraceWriter Tests
// =============================================================================

TEST_F(TraceWritersTest, MemoryWriterStoresRecords) {
    MemoryTraceWriter writer;

    writer.begin(time(0.0));
    writer.type("event1");
    writer.field("value", uint64_t{42});
    writer.end();

    writer.begin(time(1.0));
    writer.type("event2");
    writer.field("data", 3.14);
    writer.end();

    const auto& records = writer.records();
    ASSERT_EQ(records.size(), 2u);

    EXPECT_DOUBLE_EQ(records[0].time, 0.0);
    EXPECT_EQ(records[0].type, "event1");
    EXPECT_EQ(std::get<uint64_t>(records[0].fields.at("value")), 42u);

    EXPECT_DOUBLE_EQ(records[1].time, 1.0);
    EXPECT_EQ(records[1].type, "event2");
    EXPECT_DOUBLE_EQ(std::get<double>(records[1].fields.at("data")), 3.14);
}

TEST_F(TraceWritersTest, MemoryWriterClear) {
    MemoryTraceWriter writer;

    writer.begin(time(0.0));
    writer.type("test");
    writer.end();

    ASSERT_EQ(writer.records().size(), 1u);

    writer.clear();
    EXPECT_EQ(writer.records().size(), 0u);

    // Can add more records after clear
    writer.begin(time(1.0));
    writer.type("after_clear");
    writer.end();

    ASSERT_EQ(writer.records().size(), 1u);
    EXPECT_EQ(writer.records()[0].type, "after_clear");
}

TEST_F(TraceWritersTest, MemoryWriterAllFieldTypes) {
    MemoryTraceWriter writer;

    writer.begin(time(5.5));
    writer.type("test");
    writer.field("uint_field", uint64_t{100});
    writer.field("double_field", 2.718);
    writer.field("string_field", "hello");
    writer.end();

    const auto& records = writer.records();
    ASSERT_EQ(records.size(), 1u);

    const auto& rec = records[0];
    EXPECT_DOUBLE_EQ(rec.time, 5.5);
    EXPECT_EQ(rec.type, "test");
    EXPECT_EQ(std::get<uint64_t>(rec.fields.at("uint_field")), 100u);
    EXPECT_DOUBLE_EQ(std::get<double>(rec.fields.at("double_field")), 2.718);
    EXPECT_EQ(std::get<std::string>(rec.fields.at("string_field")), "hello");
}

TEST_F(TraceWritersTest, MemoryWriterEmptyRecords) {
    MemoryTraceWriter writer;
    EXPECT_TRUE(writer.records().empty());
}

TEST_F(TraceWritersTest, MemoryWriterRecordWithNoFields) {
    MemoryTraceWriter writer;

    writer.begin(time(0.0));
    writer.type("empty_event");
    writer.end();

    ASSERT_EQ(writer.records().size(), 1u);
    EXPECT_EQ(writer.records()[0].type, "empty_event");
    EXPECT_TRUE(writer.records()[0].fields.empty());
}
