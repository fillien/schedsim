#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/single_scheduler_allocator.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class DeadlineMissTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("cpu", 1.0);
        auto& cd = engine_.platform().add_clock_domain(Frequency{2000.0}, Frequency{2000.0});
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });
        proc_ = &engine_.platform().add_processor(pt, cd, pd);
    }

    TimePoint time(double seconds) {
        return time_from_seconds(seconds);
    }

    Engine engine_;
    Processor* proc_{nullptr};
};

// =============================================================================
// StopSimulation policy on processor-based miss
// =============================================================================

TEST_F(DeadlineMissTest, StopSimulationStopsEngine) {
    // Task with exec time > deadline forces a deadline miss on the processor
    // relative_deadline=5, WCET=5, but actual exec will be 8 (exceeds deadline)
    auto& task = engine_.platform().add_task(
        duration_from_seconds(20.0),  // period
        duration_from_seconds(5.0),   // relative deadline
        duration_from_seconds(5.0));  // wcet
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.set_deadline_miss_policy(DeadlineMissPolicy::StopSimulation);
    sched.add_server(task, duration_from_seconds(5.0), duration_from_seconds(20.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    bool handler_called = false;
    sched.set_deadline_miss_handler([&](Processor&, Job&) {
        handler_called = true;
    });

    // Job arrives at t=0 with exec_time=8 > deadline=5
    // At t=5, deadline miss fires; StopSimulation should halt the engine
    engine_.schedule_job_arrival(task, time(0.0), duration_from_seconds(8.0));

    // Run well past deadline — engine should stop early
    engine_.run(time(100.0));

    EXPECT_TRUE(handler_called);
    // Engine should have stopped at the deadline miss time (t=5)
    EXPECT_EQ(engine_.time(), time(5.0));
    EXPECT_TRUE(engine_.stop_requested());
}

// =============================================================================
// Queued job deadline miss detection
// =============================================================================

TEST_F(DeadlineMissTest, QueuedMissWithOverloadedScheduler) {
    // 1 processor, 2 tasks. Task1 has long exec that monopolizes the CPU.
    // Task2 arrives but can't run because task1 has an earlier CBS deadline.
    // Task2's job absolute_deadline passes while it waits.
    auto& task1 = engine_.platform().add_task(
        duration_from_seconds(5.0),   // period
        duration_from_seconds(5.0),   // relative deadline
        duration_from_seconds(4.0));  // wcet
    auto& task2 = engine_.platform().add_task(
        duration_from_seconds(10.0),  // period
        duration_from_seconds(3.0),   // relative deadline (arrives at t=0, deadline at t=3)
        duration_from_seconds(1.0));  // wcet
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.set_deadline_miss_policy(DeadlineMissPolicy::Continue);
    // Task1 server: budget=4, period=5 → server deadline at t=5
    // Task2 server: budget=1, period=10 → server deadline at t=10
    sched.add_server_unchecked(task1, duration_from_seconds(4.0), duration_from_seconds(5.0));
    sched.add_server_unchecked(task2, duration_from_seconds(1.0), duration_from_seconds(10.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    bool queued_miss_detected = false;
    std::size_t missed_task_id = 0;
    sched.set_queued_deadline_miss_handler([&](Job& job) {
        queued_miss_detected = true;
        missed_task_id = job.task().id();
    });

    // Both arrive at t=0.
    // Task1 CBS deadline=5, Task2 CBS deadline=10.
    // EDF dispatches task1 first (earlier CBS deadline).
    // Task2 sits in queue. Its job absolute deadline = 0 + 3 = 3.
    // At t=3, queued deadline timer fires for task2.
    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(4.0));
    engine_.schedule_job_arrival(task2, time(0.0), duration_from_seconds(1.0));

    engine_.run(time(15.0));

    EXPECT_TRUE(queued_miss_detected);
    EXPECT_EQ(missed_task_id, task2.id());
}

// =============================================================================
// Queued timer cancelled on dispatch
// =============================================================================

TEST_F(DeadlineMissTest, QueuedJobTimerCancelledOnDispatch) {
    // Similar to above but task1 finishes before task2's absolute deadline,
    // so task2 gets dispatched and the queued timer is cancelled.
    auto& task1 = engine_.platform().add_task(
        duration_from_seconds(5.0),
        duration_from_seconds(5.0),
        duration_from_seconds(1.0));
    auto& task2 = engine_.platform().add_task(
        duration_from_seconds(10.0),
        duration_from_seconds(8.0),   // Long relative deadline — won't miss
        duration_from_seconds(1.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.set_deadline_miss_policy(DeadlineMissPolicy::AbortJob);
    sched.add_server_unchecked(task1, duration_from_seconds(1.0), duration_from_seconds(5.0));
    sched.add_server_unchecked(task2, duration_from_seconds(1.0), duration_from_seconds(10.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    bool queued_miss_detected = false;
    sched.set_queued_deadline_miss_handler([&](Job&) {
        queued_miss_detected = true;
    });

    // Task1 CBS deadline=5, Task2 CBS deadline=10 → task1 runs first.
    // Task1 finishes at t=1, task2 dispatched.
    // Task2 absolute deadline = 0+8 = 8 → well after dispatch at t=1.
    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(1.0));
    engine_.schedule_job_arrival(task2, time(0.0), duration_from_seconds(1.0));

    engine_.run(time(15.0));

    // Queued timer should have been cancelled when task2 was dispatched
    EXPECT_FALSE(queued_miss_detected);
}

// =============================================================================
// Preempted job deadline monitored while in queue
// =============================================================================

TEST_F(DeadlineMissTest, PreemptedJobDeadlineMissDetected) {
    // Task1 runs, task2 arrives with earlier CBS deadline and preempts.
    // Task1 returns to queue. Task1's absolute deadline passes while queued.
    auto& task1 = engine_.platform().add_task(
        duration_from_seconds(10.0),
        duration_from_seconds(4.0),   // Tight absolute deadline at t=4
        duration_from_seconds(5.0));
    auto& task2 = engine_.platform().add_task(
        duration_from_seconds(3.0),   // period=3 → CBS deadline at t=3+3=6 when arrives at t=3
        duration_from_seconds(3.0),
        duration_from_seconds(2.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.set_deadline_miss_policy(DeadlineMissPolicy::Continue);
    // Task1: budget=5, period=10 → CBS deadline at t=10
    // Task2: budget=2, period=3 → CBS deadline at t=3+3=6 when activated at t=3
    sched.add_server_unchecked(task1, duration_from_seconds(5.0), duration_from_seconds(10.0));
    sched.add_server_unchecked(task2, duration_from_seconds(2.0), duration_from_seconds(3.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    bool queued_miss_detected = false;
    std::size_t missed_task_id = 0;
    sched.set_queued_deadline_miss_handler([&](Job& job) {
        queued_miss_detected = true;
        missed_task_id = job.task().id();
    });

    // Task1 arrives at t=0 (exec=5), CBS deadline=10. Dispatched immediately.
    // Task2 arrives at t=3 (exec=2), CBS deadline=6.
    // Task2 CBS deadline (6) < Task1 CBS deadline (10) → task2 preempts task1 at t=3.
    // Task1 returns to queue. Task1 absolute deadline = 0+4 = 4.
    // At t=4, queued deadline timer fires for task1.
    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(5.0));
    engine_.schedule_job_arrival(task2, time(3.0), duration_from_seconds(2.0));

    engine_.run(time(15.0));

    EXPECT_TRUE(queued_miss_detected);
    EXPECT_EQ(missed_task_id, task1.id());
}

// =============================================================================
// Queued miss with AbortJob policy
// =============================================================================

TEST_F(DeadlineMissTest, QueuedMissAbortJobPolicy) {
    auto& task1 = engine_.platform().add_task(
        duration_from_seconds(5.0),
        duration_from_seconds(5.0),
        duration_from_seconds(4.0));
    auto& task2 = engine_.platform().add_task(
        duration_from_seconds(10.0),
        duration_from_seconds(3.0),
        duration_from_seconds(1.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.set_deadline_miss_policy(DeadlineMissPolicy::AbortJob);
    sched.add_server_unchecked(task1, duration_from_seconds(4.0), duration_from_seconds(5.0));
    sched.add_server_unchecked(task2, duration_from_seconds(1.0), duration_from_seconds(10.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    bool miss_detected = false;
    sched.set_queued_deadline_miss_handler([&](Job&) { miss_detected = true; });

    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(4.0));
    engine_.schedule_job_arrival(task2, time(0.0), duration_from_seconds(1.0));

    engine_.run(time(15.0));

    EXPECT_TRUE(miss_detected);
    // After abort, the task2 server should have no pending jobs (it was the only one)
    CbsServer* server2 = sched.find_server(task2);
    ASSERT_NE(server2, nullptr);
    EXPECT_EQ(server2->state(), CbsServer::State::Inactive);
}

// =============================================================================
// Queued miss with AbortTask policy
// =============================================================================

TEST_F(DeadlineMissTest, QueuedMissAbortTaskPolicy) {
    auto& task1 = engine_.platform().add_task(
        duration_from_seconds(5.0),
        duration_from_seconds(5.0),
        duration_from_seconds(4.0));
    auto& task2 = engine_.platform().add_task(
        duration_from_seconds(10.0),
        duration_from_seconds(3.0),
        duration_from_seconds(1.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.set_deadline_miss_policy(DeadlineMissPolicy::AbortTask);
    sched.add_server_unchecked(task1, duration_from_seconds(4.0), duration_from_seconds(5.0));
    sched.add_server_unchecked(task2, duration_from_seconds(1.0), duration_from_seconds(10.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    bool miss_detected = false;
    sched.set_queued_deadline_miss_handler([&](Job&) { miss_detected = true; });

    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(4.0));
    engine_.schedule_job_arrival(task2, time(0.0), duration_from_seconds(1.0));

    engine_.run(time(15.0));

    EXPECT_TRUE(miss_detected);
    // After AbortTask, the task2 mapping should be removed
    CbsServer* server2 = sched.find_server(task2);
    EXPECT_EQ(server2, nullptr);
}

// =============================================================================
// Queued miss with StopSimulation policy
// =============================================================================

TEST_F(DeadlineMissTest, QueuedMissStopSimulationPolicy) {
    auto& task1 = engine_.platform().add_task(
        duration_from_seconds(5.0),
        duration_from_seconds(5.0),
        duration_from_seconds(4.0));
    auto& task2 = engine_.platform().add_task(
        duration_from_seconds(10.0),
        duration_from_seconds(3.0),
        duration_from_seconds(1.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.set_deadline_miss_policy(DeadlineMissPolicy::StopSimulation);
    sched.add_server_unchecked(task1, duration_from_seconds(4.0), duration_from_seconds(5.0));
    sched.add_server_unchecked(task2, duration_from_seconds(1.0), duration_from_seconds(10.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    bool miss_detected = false;
    sched.set_queued_deadline_miss_handler([&](Job&) { miss_detected = true; });

    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(4.0));
    engine_.schedule_job_arrival(task2, time(0.0), duration_from_seconds(1.0));

    engine_.run(time(100.0));

    EXPECT_TRUE(miss_detected);
    EXPECT_TRUE(engine_.stop_requested());
    // Engine should stop at t=3 (task2's deadline)
    EXPECT_EQ(engine_.time(), time(3.0));
}

// =============================================================================
// Queued miss with Continue policy — job stays queued
// =============================================================================

TEST_F(DeadlineMissTest, QueuedMissContinuePolicy) {
    auto& task1 = engine_.platform().add_task(
        duration_from_seconds(5.0),
        duration_from_seconds(5.0),
        duration_from_seconds(4.0));
    auto& task2 = engine_.platform().add_task(
        duration_from_seconds(10.0),
        duration_from_seconds(3.0),
        duration_from_seconds(1.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.set_deadline_miss_policy(DeadlineMissPolicy::Continue);
    sched.add_server_unchecked(task1, duration_from_seconds(4.0), duration_from_seconds(5.0));
    sched.add_server_unchecked(task2, duration_from_seconds(1.0), duration_from_seconds(10.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    bool miss_detected = false;
    sched.set_queued_deadline_miss_handler([&](Job&) { miss_detected = true; });

    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(4.0));
    engine_.schedule_job_arrival(task2, time(0.0), duration_from_seconds(1.0));

    engine_.run(time(15.0));

    EXPECT_TRUE(miss_detected);
    // With Continue policy, the job stays queued and eventually runs.
    // Task1 finishes at t=4, then task2 runs from t=4 to t=5.
    // Server should end up Inactive (all jobs done).
    CbsServer* server2 = sched.find_server(task2);
    ASSERT_NE(server2, nullptr);
    EXPECT_EQ(server2->state(), CbsServer::State::Inactive);
}
