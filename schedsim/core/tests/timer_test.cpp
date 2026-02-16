#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>

#include <gtest/gtest.h>

#include <vector>

using namespace schedsim::core;

class TimerTest : public ::testing::Test {
protected:
    TimePoint time(double seconds) {
        return time_from_seconds(seconds);
    }
};

TEST_F(TimerTest, BasicTimerFires) {
    Engine engine;
    bool fired = false;

    engine.add_timer(time(1.0), [&fired]() { fired = true; });

    EXPECT_FALSE(fired);

    engine.run();

    EXPECT_TRUE(fired);
    EXPECT_EQ(engine.time(), time(1.0));
}

TEST_F(TimerTest, TimerWithPriority) {
    Engine engine;
    std::vector<int> order;

    // Add timers at same time with different priorities
    engine.add_timer(time(1.0), EventPriority::TIMER_DEFAULT, [&order]() { order.push_back(3); });
    engine.add_timer(time(1.0), EventPriority::JOB_ARRIVAL, [&order]() { order.push_back(2); });
    engine.add_timer(time(1.0), EventPriority::JOB_COMPLETION, [&order]() { order.push_back(1); });

    engine.run();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1); // JOB_COMPLETION fires first
    EXPECT_EQ(order[1], 2); // JOB_ARRIVAL fires second
    EXPECT_EQ(order[2], 3); // TIMER_DEFAULT fires last
}

TEST_F(TimerTest, TimerOrderBySequence) {
    Engine engine;
    std::vector<int> order;

    // Add timers at same time with same priority
    engine.add_timer(time(1.0), 0, [&order]() { order.push_back(1); });
    engine.add_timer(time(1.0), 0, [&order]() { order.push_back(2); });
    engine.add_timer(time(1.0), 0, [&order]() { order.push_back(3); });

    engine.run();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1); // First inserted fires first
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST_F(TimerTest, TimerCancellation) {
    Engine engine;
    bool fired = false;

    TimerId timer_id = engine.add_timer(time(1.0), [&fired]() { fired = true; });

    EXPECT_TRUE(timer_id.valid());

    engine.cancel_timer(timer_id);

    EXPECT_FALSE(timer_id.valid());

    engine.run();

    EXPECT_FALSE(fired);
}

TEST_F(TimerTest, CancelInvalidTimer) {
    Engine engine;

    TimerId invalid_timer;
    EXPECT_FALSE(invalid_timer.valid());

    // Should be a no-op, not throw
    engine.cancel_timer(invalid_timer);

    EXPECT_FALSE(invalid_timer.valid());
}

TEST_F(TimerTest, DoubleCancelIsNoOp) {
    Engine engine;

    TimerId timer_id = engine.add_timer(time(1.0), []() {});

    engine.cancel_timer(timer_id);
    EXPECT_FALSE(timer_id.valid());

    // Second cancel should be a no-op
    engine.cancel_timer(timer_id);
    EXPECT_FALSE(timer_id.valid());
}

TEST_F(TimerTest, TimerSchedulesTimer) {
    Engine engine;
    bool second_fired = false;

    engine.add_timer(time(1.0), [&engine, &second_fired, this]() {
        engine.add_timer(time(2.0), [&second_fired]() {
            second_fired = true;
        });
    });

    engine.run();

    EXPECT_TRUE(second_fired);
    EXPECT_EQ(engine.time(), time(2.0));
}

TEST_F(TimerTest, TimerAtCurrentTimeAllowed) {
    Engine engine;
    bool fired = false;

    // Timer at current time should NOT throw (was changed to support deadline timers)
    EXPECT_NO_THROW(
        engine.add_timer(time(0.0), [&fired]() { fired = true; })
    );

    engine.run();
    EXPECT_TRUE(fired);
}

TEST_F(TimerTest, TimerInPastThrows) {
    Engine engine;

    // Advance time first
    engine.add_timer(time(5.0), []() {});
    engine.run();
    EXPECT_EQ(engine.time(), time(5.0));

    // Timer in the past should throw
    EXPECT_THROW(
        engine.add_timer(time(3.0), []() {}),
        InvalidStateError
    );
}

TEST_F(TimerTest, TimerAtCurrentTimeAfterAdvance) {
    Engine engine;

    engine.add_timer(time(5.0), []() {});
    engine.run();

    EXPECT_EQ(engine.time(), time(5.0));

    // Timer at current time (5.0) should be allowed
    bool fired = false;
    EXPECT_NO_THROW(
        engine.add_timer(time(5.0), [&fired]() { fired = true; })
    );

    engine.run();
    EXPECT_TRUE(fired);
}

TEST_F(TimerTest, MultipleTimersAtDifferentTimes) {
    Engine engine;
    std::vector<double> fire_times;

    engine.add_timer(time(3.0), [&fire_times, &engine]() {
        fire_times.push_back(time_to_seconds(engine.time()));
    });
    engine.add_timer(time(1.0), [&fire_times, &engine]() {
        fire_times.push_back(time_to_seconds(engine.time()));
    });
    engine.add_timer(time(2.0), [&fire_times, &engine]() {
        fire_times.push_back(time_to_seconds(engine.time()));
    });

    engine.run();

    ASSERT_EQ(fire_times.size(), 3u);
    EXPECT_DOUBLE_EQ(fire_times[0], 1.0);
    EXPECT_DOUBLE_EQ(fire_times[1], 2.0);
    EXPECT_DOUBLE_EQ(fire_times[2], 3.0);
}

TEST_F(TimerTest, TimerIdDefaultConstruction) {
    TimerId timer_id;

    EXPECT_FALSE(timer_id.valid());
    EXPECT_FALSE(static_cast<bool>(timer_id));
}

TEST_F(TimerTest, TimerIdBoolConversion) {
    Engine engine;

    TimerId timer_id = engine.add_timer(time(1.0), []() {});

    EXPECT_TRUE(timer_id.valid());
    EXPECT_TRUE(static_cast<bool>(timer_id));

    engine.cancel_timer(timer_id);

    EXPECT_FALSE(timer_id.valid());
    EXPECT_FALSE(static_cast<bool>(timer_id));
}

TEST_F(TimerTest, TimerIdClearMarksInvalid) {
    Engine engine;

    TimerId timer_id = engine.add_timer(time(1.0), []() {});

    EXPECT_TRUE(timer_id.valid());

    timer_id.clear();

    EXPECT_FALSE(timer_id.valid());
    EXPECT_FALSE(static_cast<bool>(timer_id));
}

TEST_F(TimerTest, CancelAfterClearIsNoOp) {
    Engine engine;
    bool fired = false;

    TimerId timer_id = engine.add_timer(time(1.0), [&fired]() { fired = true; });

    timer_id.clear();

    // Cancelling a cleared timer should be a no-op (not crash)
    engine.cancel_timer(timer_id);

    EXPECT_FALSE(timer_id.valid());

    // Timer should still fire (clear only marks TimerId invalid, doesn't remove from queue)
    engine.run();

    EXPECT_TRUE(fired);
}

TEST_F(TimerTest, TimerCallbackCancelsOtherTimer) {
    Engine engine;
    bool timer1_fired = false;
    bool timer2_fired = false;

    TimerId timer2_id;

    // Timer 1 at t=1.0 cancels timer 2
    engine.add_timer(time(1.0), [&]() {
        timer1_fired = true;
        engine.cancel_timer(timer2_id);
    });

    // Timer 2 at t=2.0
    timer2_id = engine.add_timer(time(2.0), [&]() {
        timer2_fired = true;
    });

    engine.run();

    EXPECT_TRUE(timer1_fired);
    EXPECT_FALSE(timer2_fired);  // Timer 2 was cancelled
}
