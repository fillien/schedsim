#include <schedsim/core/engine.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

class EngineStopTest : public ::testing::Test {
protected:
    TimePoint time(double seconds) {
        return time_from_seconds(seconds);
    }
};

TEST_F(EngineStopTest, RequestStopHaltsSimulation) {
    Engine engine;

    int counter = 0;
    engine.add_timer(time(1.0), [&]() { counter++; });
    engine.add_timer(time(2.0), [&]() {
        counter++;
        engine.request_stop();
    });
    engine.add_timer(time(3.0), [&]() { counter++; });  // Should NOT fire

    engine.run();

    // Only 2 timers should have fired (t=1 and t=2); t=3 skipped due to stop
    EXPECT_EQ(counter, 2);
    EXPECT_EQ(engine.time(), time(2.0));
}

TEST_F(EngineStopTest, RequestStopCompletesCurrentTimestep) {
    Engine engine;

    int counter = 0;
    // Two events at the same time — both should fire even if stop is requested
    engine.add_timer(time(1.0), 0, [&]() {
        counter++;
        engine.request_stop();
    });
    engine.add_timer(time(1.0), 1, [&]() {
        counter++;  // Same timestep — must still fire
    });
    engine.add_timer(time(2.0), [&]() {
        counter++;  // Different timestep — should NOT fire
    });

    engine.run();

    EXPECT_EQ(counter, 2);  // Both t=1.0 events fired
    EXPECT_EQ(engine.time(), time(1.0));
}

TEST_F(EngineStopTest, RequestStopAutoResets) {
    Engine engine;

    // First run: stop at t=2
    engine.add_timer(time(1.0), []() {});
    engine.add_timer(time(2.0), [&]() { engine.request_stop(); });
    engine.add_timer(time(3.0), []() {});  // Not reached

    engine.run();
    EXPECT_EQ(engine.time(), time(2.0));
    EXPECT_TRUE(engine.stop_requested());

    // Second run: flag should auto-reset, all new events fire
    int counter = 0;
    engine.add_timer(time(4.0), [&]() { counter++; });
    engine.add_timer(time(5.0), [&]() { counter++; });

    engine.run();

    EXPECT_EQ(counter, 2);  // Both fired — stop_requested was reset
    EXPECT_EQ(engine.time(), time(5.0));
    EXPECT_FALSE(engine.stop_requested());
}

TEST_F(EngineStopTest, RequestStopWithRunUntil) {
    Engine engine;

    engine.add_timer(time(1.0), [&]() { engine.request_stop(); });
    engine.add_timer(time(5.0), []() {});  // Should NOT fire

    engine.run(time(10.0));

    EXPECT_EQ(engine.time(), time(1.0));
}

TEST_F(EngineStopTest, RequestStopWithCondition) {
    Engine engine;

    int counter = 0;
    engine.add_timer(time(1.0), [&]() { counter++; });
    engine.add_timer(time(2.0), [&]() {
        counter++;
        engine.request_stop();
    });
    engine.add_timer(time(3.0), [&]() { counter++; });

    // Condition alone wouldn't stop until counter >= 10
    engine.run([&]() { return counter >= 10; });

    EXPECT_EQ(counter, 2);  // request_stop() took effect first
}

TEST_F(EngineStopTest, StopRequestedQueryable) {
    Engine engine;

    EXPECT_FALSE(engine.stop_requested());

    engine.request_stop();
    EXPECT_TRUE(engine.stop_requested());
}
