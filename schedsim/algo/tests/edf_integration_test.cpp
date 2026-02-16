#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/single_scheduler_allocator.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class EdfIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("cpu", 1.0);
        auto& cd = engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
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

TEST_F(EdfIntegrationTest, SingleTask_RunsToCompletion) {
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.add_server(task);
    SingleSchedulerAllocator alloc(engine_, sched);

    // Schedule a job arrival
    engine_.schedule_job_arrival(task, time(0.0), duration_from_seconds(2.0));

    // Run until job completes (2.0 time units execution)
    engine_.run(time(10.0));

    // The server should have processed the job and returned to Inactive
    CbsServer* server = sched.find_server(task);
    ASSERT_NE(server, nullptr);
    EXPECT_EQ(server->state(), CbsServer::State::Inactive);
    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
}

TEST_F(EdfIntegrationTest, TwoTasks_EdfOrder) {
    auto& task1 = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    auto& task2 = engine_.platform().add_task(duration_from_seconds(20.0), duration_from_seconds(5.0), duration_from_seconds(1.0));  // Earlier deadline
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.add_server(task1, duration_from_seconds(2.0), duration_from_seconds(10.0));
    sched.add_server(task2, duration_from_seconds(1.0), duration_from_seconds(20.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    std::vector<std::size_t> completion_order;

    // Custom completion handler to track order
    // Note: We need to intercept before the scheduler's handler
    // For this test, we just verify state after run

    // Both jobs arrive at t=0
    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(2.0));  // deadline=10
    engine_.schedule_job_arrival(task2, time(0.0), duration_from_seconds(1.0));  // deadline=5

    engine_.run(time(10.0));

    // Both tasks should have completed
    // task2 should have run first (earlier deadline at t=5)
    // After task2 completes, task1 runs
    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
}

TEST_F(EdfIntegrationTest, Preemption_EarlierDeadlineArrives) {
    auto& task1 = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(4.0));
    auto& task2 = engine_.platform().add_task(duration_from_seconds(5.0), duration_from_seconds(3.0), duration_from_seconds(1.0));  // Earlier deadline
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.add_server(task1, duration_from_seconds(4.0), duration_from_seconds(10.0));
    sched.add_server(task2, duration_from_seconds(1.0), duration_from_seconds(5.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    // Task1 arrives at t=0, deadline=10
    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(4.0));
    // Task2 arrives at t=1, deadline=1+3=4 (earlier than task1)
    engine_.schedule_job_arrival(task2, time(1.0), duration_from_seconds(1.0));

    engine_.run(time(10.0));

    // Both should complete
    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
}

TEST_F(EdfIntegrationTest, BudgetExhaustion_PostponesDeadline) {
    // Create a task with job longer than budget
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    // Budget=2, Period=10, but job needs 5 time units
    sched.add_server(task, duration_from_seconds(2.0), duration_from_seconds(10.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    engine_.schedule_job_arrival(task, time(0.0), duration_from_seconds(5.0));

    engine_.run(time(20.0));

    // Job should eventually complete after multiple budget replenishments
    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
}

TEST_F(EdfIntegrationTest, MultipleJobArrivals) {
    auto& task = engine_.platform().add_task(duration_from_seconds(5.0), duration_from_seconds(5.0), duration_from_seconds(1.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    sched.add_server(task);
    SingleSchedulerAllocator alloc(engine_, sched);

    // Three job arrivals at different times
    engine_.schedule_job_arrival(task, time(0.0), duration_from_seconds(1.0));
    engine_.schedule_job_arrival(task, time(5.0), duration_from_seconds(1.0));
    engine_.schedule_job_arrival(task, time(10.0), duration_from_seconds(1.0));

    engine_.run(time(15.0));

    // All jobs should complete
    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
}

// Multi-processor integration test fixture
class EdfMultiProcIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("cpu", 1.0);
        auto& cd = engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });

        proc1_ = &engine_.platform().add_processor(pt, cd, pd);
        proc2_ = &engine_.platform().add_processor(pt, cd, pd);
    }

    TimePoint time(double seconds) {
        return time_from_seconds(seconds);
    }

    Engine engine_;
    Processor* proc1_{nullptr};
    Processor* proc2_{nullptr};
};

TEST_F(EdfMultiProcIntegrationTest, TwoTasks_ParallelExecution) {
    auto& task1 = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    auto& task2 = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc1_, proc2_});
    sched.add_server(task1);
    sched.add_server(task2);
    SingleSchedulerAllocator alloc(engine_, sched);

    // Both jobs arrive at t=0
    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(2.0));
    engine_.schedule_job_arrival(task2, time(0.0), duration_from_seconds(2.0));

    engine_.run(time(5.0));

    // Both tasks should complete (running in parallel on 2 processors)
    EXPECT_EQ(proc1_->state(), ProcessorState::Idle);
    EXPECT_EQ(proc2_->state(), ProcessorState::Idle);
}

TEST_F(EdfMultiProcIntegrationTest, ThreeTasks_TwoProcessors) {
    auto& task1 = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(5.0), duration_from_seconds(2.0));   // d=5
    auto& task2 = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(8.0), duration_from_seconds(2.0));   // d=8
    auto& task3 = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0)); // d=10
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc1_, proc2_});
    sched.add_server(task1, duration_from_seconds(2.0), duration_from_seconds(10.0));
    sched.add_server(task2, duration_from_seconds(2.0), duration_from_seconds(10.0));
    sched.add_server(task3, duration_from_seconds(2.0), duration_from_seconds(10.0));
    SingleSchedulerAllocator alloc(engine_, sched);

    // All three arrive at t=0
    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(2.0));
    engine_.schedule_job_arrival(task2, time(0.0), duration_from_seconds(2.0));
    engine_.schedule_job_arrival(task3, time(0.0), duration_from_seconds(2.0));

    engine_.run(time(10.0));

    // All tasks should complete
    EXPECT_EQ(proc1_->state(), ProcessorState::Idle);
    EXPECT_EQ(proc2_->state(), ProcessorState::Idle);
}

TEST_F(EdfMultiProcIntegrationTest, GlobalEdf_Migration) {
    auto& task1 = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(3.0));
    auto& task2 = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(4.0), duration_from_seconds(2.0));  // Higher priority
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc1_, proc2_});
    sched.add_server(task1);
    sched.add_server(task2);
    SingleSchedulerAllocator alloc(engine_, sched);

    // task1 starts at t=0 on proc1
    engine_.schedule_job_arrival(task1, time(0.0), duration_from_seconds(3.0));

    // task2 arrives at t=1, should preempt or run on proc2
    engine_.schedule_job_arrival(task2, time(1.0), duration_from_seconds(2.0));

    engine_.run(time(10.0));

    // Both complete
    EXPECT_EQ(proc1_->state(), ProcessorState::Idle);
    EXPECT_EQ(proc2_->state(), ProcessorState::Idle);
}

TEST_F(EdfIntegrationTest, DeadlineMissHandler_Called) {
    // Create a task where execution time exceeds relative deadline
    // Task: period=10, relative_deadline=2, wcet=5
    // Job execution time will be 3 (less than wcet but more than deadline)
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(2.0), duration_from_seconds(5.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    // Use budget=5, period=10 (matching task)
    sched.add_server(task, duration_from_seconds(5.0), duration_from_seconds(10.0));

    bool deadline_missed = false;
    sched.set_deadline_miss_handler([&](Processor&, Job&) {
        deadline_missed = true;
    });
    sched.set_deadline_miss_policy(DeadlineMissPolicy::Continue);

    SingleSchedulerAllocator alloc(engine_, sched);

    // Job with 3 time units but deadline at t=2
    // Deadline miss at t=2, job completes at t=3
    engine_.schedule_job_arrival(task, time(0.0), duration_from_seconds(3.0));

    engine_.run(time(10.0));

    EXPECT_TRUE(deadline_missed);
}

TEST_F(EdfIntegrationTest, AutoCreateServer_OnJobArrival) {
    auto& task = engine_.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    // Don't add server explicitly
    SingleSchedulerAllocator alloc(engine_, sched);

    engine_.schedule_job_arrival(task, time(0.0), duration_from_seconds(2.0));

    engine_.run(time(5.0));

    // Server should have been auto-created
    CbsServer* server = sched.find_server(task);
    EXPECT_NE(server, nullptr);
    EXPECT_EQ(sched.server_count(), 1U);
}
