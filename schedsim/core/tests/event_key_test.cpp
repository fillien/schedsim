#include <schedsim/core/event.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using namespace schedsim::core;

class EventKeyTest : public ::testing::Test {
protected:
    TimePoint time(double seconds) {
        return time_from_seconds(seconds);
    }
};

TEST_F(EventKeyTest, OrderByTime) {
    EventKey early{time(1.0), 0, 0};
    EventKey late{time(2.0), 0, 0};

    EXPECT_LT(early, late);
    EXPECT_GT(late, early);
    EXPECT_NE(early, late);
}

TEST_F(EventKeyTest, OrderByPriorityWhenSameTime) {
    EventKey high_priority{time(1.0), -100, 0};
    EventKey low_priority{time(1.0), 100, 0};

    EXPECT_LT(high_priority, low_priority);
    EXPECT_GT(low_priority, high_priority);
}

TEST_F(EventKeyTest, OrderBySequenceWhenSameTimePriority) {
    EventKey first{time(1.0), 0, 0};
    EventKey second{time(1.0), 0, 1};

    EXPECT_LT(first, second);
    EXPECT_GT(second, first);
}

TEST_F(EventKeyTest, Equality) {
    EventKey key1{time(1.0), 0, 0};
    EventKey key2{time(1.0), 0, 0};

    EXPECT_EQ(key1, key2);
}

TEST_F(EventKeyTest, SortingOrder) {
    // Create events in random order
    std::vector<EventKey> events{
        {time(2.0), 0, 0},    // Should be 4th
        {time(1.0), 100, 0},  // Should be 3rd
        {time(1.0), -100, 1}, // Should be 2nd
        {time(1.0), -100, 0}, // Should be 1st
        {time(3.0), -500, 0}, // Should be 5th
    };

    std::sort(events.begin(), events.end());

    EXPECT_EQ(events[0].time, time(1.0));
    EXPECT_EQ(events[0].priority, -100);
    EXPECT_EQ(events[0].sequence, 0u);

    EXPECT_EQ(events[1].time, time(1.0));
    EXPECT_EQ(events[1].priority, -100);
    EXPECT_EQ(events[1].sequence, 1u);

    EXPECT_EQ(events[2].time, time(1.0));
    EXPECT_EQ(events[2].priority, 100);

    EXPECT_EQ(events[3].time, time(2.0));

    EXPECT_EQ(events[4].time, time(3.0));
}

TEST_F(EventKeyTest, EventPriorityConstants) {
    // Verify priority ordering: completion < deadline < available < arrival < timer
    EXPECT_LT(EventPriority::JOB_COMPLETION, EventPriority::DEADLINE_MISS);
    EXPECT_LT(EventPriority::DEADLINE_MISS, EventPriority::PROCESSOR_AVAILABLE);
    EXPECT_LT(EventPriority::PROCESSOR_AVAILABLE, EventPriority::JOB_ARRIVAL);
    EXPECT_LT(EventPriority::JOB_ARRIVAL, EventPriority::TIMER_DEFAULT);
}
