#include <schedsim/core/engine.hpp>
#include <schedsim/core/trace_writer.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace schedsim::core;

// Mock TraceWriter for testing
class MockTraceWriter : public TraceWriter {
public:
    struct Record {
        TimePoint time;
        std::string type_name;
        std::vector<std::pair<std::string, std::string>> fields;
    };

    void begin(TimePoint time) override {
        current_record_.time = time;
        current_record_.type_name.clear();
        current_record_.fields.clear();
    }

    void type(std::string_view name) override {
        current_record_.type_name = std::string(name);
    }

    void field(std::string_view key, double value) override {
        current_record_.fields.emplace_back(std::string(key), std::to_string(value));
    }

    void field(std::string_view key, uint64_t value) override {
        current_record_.fields.emplace_back(std::string(key), std::to_string(value));
    }

    void field(std::string_view key, std::string_view value) override {
        current_record_.fields.emplace_back(std::string(key), std::string(value));
    }

    void end() override {
        records.push_back(current_record_);
    }

    std::vector<Record> records;

private:
    Record current_record_;
};

class TraceWriterTest : public ::testing::Test {
protected:
    TimePoint time(double seconds) {
        return TimePoint{Duration{seconds}};
    }
};

TEST_F(TraceWriterTest, NullWriterSafety) {
    Engine engine;

    // No trace writer set - should be a no-op
    engine.trace([](TraceWriter& writer) {
        writer.type("test");
    });

    // Should not crash or throw
}

TEST_F(TraceWriterTest, MockWriterBasicUsage) {
    Engine engine;
    MockTraceWriter writer;
    engine.set_trace_writer(&writer);

    engine.trace([](TraceWriter& wr) {
        wr.type("TestEvent");
        wr.field("count", uint64_t{42});
        wr.field("value", 3.14);
        wr.field("name", "hello");
    });

    ASSERT_EQ(writer.records.size(), 1u);
    EXPECT_EQ(writer.records[0].time, time(0.0));
    EXPECT_EQ(writer.records[0].type_name, "TestEvent");
    ASSERT_EQ(writer.records[0].fields.size(), 3u);
}

TEST_F(TraceWriterTest, TraceAutoTimestamp) {
    Engine engine;
    MockTraceWriter writer;
    engine.set_trace_writer(&writer);

    engine.add_timer(time(5.0), [&engine]() {
        engine.trace([](TraceWriter& wr) {
            wr.type("TimerFired");
        });
    });

    engine.run();

    ASSERT_EQ(writer.records.size(), 1u);
    EXPECT_EQ(writer.records[0].time, time(5.0));
    EXPECT_EQ(writer.records[0].type_name, "TimerFired");
}

TEST_F(TraceWriterTest, SetNullWriterDisablesTracing) {
    Engine engine;
    MockTraceWriter writer;

    engine.set_trace_writer(&writer);

    engine.trace([](TraceWriter& wr) {
        wr.type("First");
    });

    engine.set_trace_writer(nullptr);

    engine.trace([](TraceWriter& wr) {
        wr.type("Second");
    });

    // Only first trace should be recorded
    ASSERT_EQ(writer.records.size(), 1u);
    EXPECT_EQ(writer.records[0].type_name, "First");
}

TEST_F(TraceWriterTest, MultipleTraceRecords) {
    Engine engine;
    MockTraceWriter writer;
    engine.set_trace_writer(&writer);

    engine.trace([](TraceWriter& wr) {
        wr.type("Event1");
    });

    engine.trace([](TraceWriter& wr) {
        wr.type("Event2");
    });

    engine.trace([](TraceWriter& wr) {
        wr.type("Event3");
    });

    ASSERT_EQ(writer.records.size(), 3u);
    EXPECT_EQ(writer.records[0].type_name, "Event1");
    EXPECT_EQ(writer.records[1].type_name, "Event2");
    EXPECT_EQ(writer.records[2].type_name, "Event3");
}

TEST_F(TraceWriterTest, ZeroOverheadWhenNoWriter) {
    Engine engine;
    int call_count = 0;

    // No writer set - lambda should not be invoked
    engine.trace([&call_count](TraceWriter& writer) {
        call_count++;
        writer.type("test");
    });

    EXPECT_EQ(call_count, 0);
}

TEST_F(TraceWriterTest, TraceWithAllFieldTypes) {
    Engine engine;
    MockTraceWriter writer;
    engine.set_trace_writer(&writer);

    engine.trace([](TraceWriter& wr) {
        wr.type("AllFields");
        wr.field("double_field", 1.5);
        wr.field("uint64_field", uint64_t{100});
        wr.field("string_field", "test_value");
    });

    ASSERT_EQ(writer.records.size(), 1u);
    ASSERT_EQ(writer.records[0].fields.size(), 3u);

    EXPECT_EQ(writer.records[0].fields[0].first, "double_field");
    EXPECT_EQ(writer.records[0].fields[1].first, "uint64_field");
    EXPECT_EQ(writer.records[0].fields[2].first, "string_field");
    EXPECT_EQ(writer.records[0].fields[2].second, "test_value");
}
