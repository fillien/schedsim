#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

class EngineBasicTest : public ::testing::Test {
protected:
    TimePoint time(double seconds) {
        return time_from_seconds(seconds);
    }
};

TEST_F(EngineBasicTest, InitialState) {
    Engine engine;

    EXPECT_EQ(engine.time(), time(0.0));
    EXPECT_FALSE(engine.is_finalized());
}

TEST_F(EngineBasicTest, RunEmptyQueue) {
    Engine engine;

    // Should return immediately without error
    engine.run();

    EXPECT_EQ(engine.time(), time(0.0));
}

TEST_F(EngineBasicTest, RunUntilTime) {
    Engine engine;

    engine.run(time(10.0));

    EXPECT_EQ(engine.time(), time(10.0));
}

TEST_F(EngineBasicTest, RunWithCondition) {
    Engine engine;
    int counter = 0;

    // Add some timers to have events to process
    engine.add_timer(time(1.0), [&counter]() { counter++; });
    engine.add_timer(time(2.0), [&counter]() { counter++; });
    engine.add_timer(time(3.0), [&counter]() { counter++; });

    // Stop after counter reaches 2
    engine.run([&counter]() { return counter >= 2; });

    EXPECT_EQ(counter, 2);
    EXPECT_EQ(engine.time(), time(2.0));
}

TEST_F(EngineBasicTest, Finalization) {
    Engine engine;

    EXPECT_FALSE(engine.is_finalized());

    engine.finalize();

    EXPECT_TRUE(engine.is_finalized());
}

TEST_F(EngineBasicTest, FinalizedPreventsTimerRegistration) {
    Engine engine;
    engine.finalize();

    EXPECT_THROW(
        engine.add_timer(time(1.0), []() {}),
        AlreadyFinalizedError
    );
}

TEST_F(EngineBasicTest, FinalizedPreventsDeferredRegistration) {
    Engine engine;
    engine.finalize();

    EXPECT_THROW(
        engine.register_deferred([]() {}),
        AlreadyFinalizedError
    );
}

TEST_F(EngineBasicTest, RunUntilStopsAtCorrectTime) {
    Engine engine;

    bool timer_fired = false;
    engine.add_timer(time(5.0), [&timer_fired]() { timer_fired = true; });

    // Stop at time 3.0, before the timer
    engine.run(time(3.0));

    EXPECT_EQ(engine.time(), time(3.0));
    EXPECT_FALSE(timer_fired);
}

TEST_F(EngineBasicTest, RunUntilProcessesEventsAtStopTime) {
    Engine engine;

    bool timer_fired = false;
    engine.add_timer(time(5.0), [&timer_fired]() { timer_fired = true; });

    // Stop at time 5.0, at the timer
    engine.run(time(5.0));

    EXPECT_EQ(engine.time(), time(5.0));
    EXPECT_TRUE(timer_fired);
}

TEST_F(EngineBasicTest, RunUntilPastAllEvents) {
    Engine engine;

    bool timer_fired = false;
    engine.add_timer(time(2.0), [&timer_fired]() { timer_fired = true; });

    // Stop at time 10.0, after all events
    engine.run(time(10.0));

    EXPECT_EQ(engine.time(), time(10.0));
    EXPECT_TRUE(timer_fired);
}

TEST_F(EngineBasicTest, TimeAdvancesWithEvents) {
    Engine engine;

    engine.add_timer(time(5.0), []() {});

    EXPECT_EQ(engine.time(), time(0.0));

    engine.run();

    EXPECT_EQ(engine.time(), time(5.0));
}

TEST_F(EngineBasicTest, ScheduleJobArrivalPastTimeThrows) {
    Engine engine;

    auto& pt = engine.platform().add_processor_type("big", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{1000.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    engine.platform().add_processor(pt, cd, pd);
    Task task(0, duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine.platform().finalize();

    engine.set_job_arrival_handler([](Task&, Job) {});
    engine.add_timer(time(5.0), []() {});
    engine.run();

    EXPECT_EQ(engine.time(), time(5.0));

    EXPECT_THROW(
        engine.schedule_job_arrival(task, time(2.0), duration_from_seconds(1.0)),
        InvalidStateError
    );
}
