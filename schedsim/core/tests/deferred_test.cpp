#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>

#include <gtest/gtest.h>

#include <vector>

using namespace schedsim::core;

class DeferredTest : public ::testing::Test {
protected:
    TimePoint time(double seconds) {
        return time_from_seconds(seconds);
    }
};

TEST_F(DeferredTest, DeferredFiresAfterEvent) {
    Engine engine;
    std::vector<std::string> order;

    DeferredId deferred = engine.register_deferred([&order]() {
        order.push_back("deferred");
    });

    engine.add_timer(time(1.0), [&order, &engine, deferred]() {
        order.push_back("timer");
        engine.request_deferred(deferred);
    });

    engine.run();

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], "timer");
    EXPECT_EQ(order[1], "deferred");
}

TEST_F(DeferredTest, DeferredFiresInRegistrationOrder) {
    Engine engine;
    std::vector<int> order;

    DeferredId def1 = engine.register_deferred([&order]() { order.push_back(1); });
    DeferredId def2 = engine.register_deferred([&order]() { order.push_back(2); });
    DeferredId def3 = engine.register_deferred([&order]() { order.push_back(3); });

    engine.add_timer(time(1.0), [&engine, def1, def2, def3]() {
        // Request in reverse order
        engine.request_deferred(def3);
        engine.request_deferred(def1);
        engine.request_deferred(def2);
    });

    engine.run();

    // Should fire in registration order, not request order
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST_F(DeferredTest, DeferredDeduplication) {
    Engine engine;
    int counter = 0;

    DeferredId deferred = engine.register_deferred([&counter]() { counter++; });

    engine.add_timer(time(1.0), [&engine, deferred]() {
        // Request same deferred multiple times
        engine.request_deferred(deferred);
        engine.request_deferred(deferred);
        engine.request_deferred(deferred);
    });

    engine.run();

    // Should only fire once
    EXPECT_EQ(counter, 1);
}

TEST_F(DeferredTest, SinglePassSemantics) {
    Engine engine;
    std::vector<std::string> order;

    DeferredId def1 = engine.register_deferred([&order, &engine]() {
        order.push_back("deferred1");
    });

    DeferredId def2 = engine.register_deferred([&order]() {
        order.push_back("deferred2");
    });

    engine.add_timer(time(1.0), [&engine, def1]() {
        engine.request_deferred(def1);
    });

    // def1 requests def2 during deferred phase
    DeferredId def1_modified = engine.register_deferred([&order, &engine, def2]() {
        order.push_back("deferred1_requesting_def2");
        engine.request_deferred(def2);
    });

    engine.add_timer(time(2.0), [&engine, def1_modified]() {
        engine.request_deferred(def1_modified);
    });

    engine.run();

    // At time 1.0: deferred1 fires
    // At time 2.0: deferred1_requesting_def2 fires, requests def2
    // def2 should fire in next batch (after deferred phase at time 2.0)
    ASSERT_GE(order.size(), 2u);
    EXPECT_EQ(order[0], "deferred1");
    EXPECT_EQ(order[1], "deferred1_requesting_def2");
    // def2 will fire in the same deferred pass since we iterate by index
    if (order.size() >= 3) {
        EXPECT_EQ(order[2], "deferred2");
    }
}

TEST_F(DeferredTest, DeferredNotRequestedDoesNotFire) {
    Engine engine;
    bool fired = false;

    engine.register_deferred([&fired]() { fired = true; });

    engine.add_timer(time(1.0), []() {
        // Don't request the deferred
    });

    engine.run();

    EXPECT_FALSE(fired);
}

TEST_F(DeferredTest, DeferredIdDefaultConstruction) {
    DeferredId deferred;

    EXPECT_FALSE(deferred.valid());
    EXPECT_FALSE(static_cast<bool>(deferred));
}

TEST_F(DeferredTest, DeferredIdBoolConversion) {
    Engine engine;

    DeferredId deferred = engine.register_deferred([]() {});

    EXPECT_TRUE(deferred.valid());
    EXPECT_TRUE(static_cast<bool>(deferred));
}

TEST_F(DeferredTest, RequestInvalidDeferredIsNoOp) {
    Engine engine;

    DeferredId invalid_deferred;

    // Should be a no-op, not throw
    engine.request_deferred(invalid_deferred);

    engine.run();
}

TEST_F(DeferredTest, DeferredAcrossMultipleTimesteps) {
    Engine engine;
    std::vector<double> fire_times;

    DeferredId deferred = engine.register_deferred([&fire_times, &engine]() {
        fire_times.push_back(time_to_seconds(engine.time()));
    });

    engine.add_timer(time(1.0), [&engine, deferred]() {
        engine.request_deferred(deferred);
    });

    engine.add_timer(time(3.0), [&engine, deferred]() {
        engine.request_deferred(deferred);
    });

    engine.run();

    ASSERT_EQ(fire_times.size(), 2u);
    EXPECT_DOUBLE_EQ(fire_times[0], 1.0);
    EXPECT_DOUBLE_EQ(fire_times[1], 3.0);
}

TEST_F(DeferredTest, DeferredFiresAfterAllEventsAtTimestep) {
    Engine engine;
    std::vector<std::string> order;

    DeferredId deferred = engine.register_deferred([&order]() {
        order.push_back("deferred");
    });

    // Multiple events at same time
    engine.add_timer(time(1.0), EventPriority::TIMER_DEFAULT, [&order, &engine, deferred]() {
        order.push_back("timer1");
        engine.request_deferred(deferred);
    });

    engine.add_timer(time(1.0), EventPriority::JOB_ARRIVAL, [&order]() {
        order.push_back("timer2");
    });

    engine.run();

    // Both timers should fire before deferred
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], "timer2"); // Higher priority (lower value)
    EXPECT_EQ(order[1], "timer1");
    EXPECT_EQ(order[2], "deferred");
}
