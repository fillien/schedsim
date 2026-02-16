#include <schedsim/algo/cbs_server.hpp>

#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/types.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class CbsServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        task_ = std::make_unique<Task>(0, Duration{10.0}, Duration{10.0}, Duration{2.0});
    }

    TimePoint time(double seconds) {
        return TimePoint{Duration{seconds}};
    }

    Job make_job(Duration exec_time = Duration{2.0}) {
        TimePoint deadline = time(10.0);
        return Job(*task_, exec_time, deadline);
    }

    std::unique_ptr<Task> task_;
};

TEST_F(CbsServerTest, Construction) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    EXPECT_EQ(server.budget().count(), 2.0);
    EXPECT_EQ(server.period().count(), 10.0);
    EXPECT_DOUBLE_EQ(server.utilization(), 0.2);
    EXPECT_EQ(server.state(), CbsServer::State::Inactive);
    EXPECT_EQ(server.overrun_policy(), CbsServer::OverrunPolicy::Queue);
}

TEST_F(CbsServerTest, ConstructionWithPolicy) {
    CbsServer server(0, Duration{2.0}, Duration{10.0}, CbsServer::OverrunPolicy::Skip);

    EXPECT_EQ(server.overrun_policy(), CbsServer::OverrunPolicy::Skip);
}

TEST_F(CbsServerTest, JobQueue_EnqueueDequeue) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    EXPECT_FALSE(server.has_pending_jobs());
    EXPECT_EQ(server.job_queue_size(), 0U);

    server.enqueue_job(make_job());

    EXPECT_TRUE(server.has_pending_jobs());
    EXPECT_EQ(server.job_queue_size(), 1U);
    EXPECT_NE(server.current_job(), nullptr);

    Job dequeued = server.dequeue_job();

    EXPECT_FALSE(server.has_pending_jobs());
    EXPECT_EQ(server.job_queue_size(), 0U);
}

TEST_F(CbsServerTest, JobQueue_MultipleJobs) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    server.enqueue_job(make_job(Duration{1.0}));
    server.enqueue_job(make_job(Duration{2.0}));
    server.enqueue_job(make_job(Duration{3.0}));

    EXPECT_EQ(server.job_queue_size(), 3U);

    // Jobs are dequeued in FIFO order
    Job first = server.dequeue_job();
    EXPECT_EQ(first.total_work().count(), 1.0);

    Job second = server.dequeue_job();
    EXPECT_EQ(second.total_work().count(), 2.0);

    Job third = server.dequeue_job();
    EXPECT_EQ(third.total_work().count(), 3.0);

    EXPECT_FALSE(server.has_pending_jobs());
}

TEST_F(CbsServerTest, StateTransition_InactiveToReady) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    server.enqueue_job(make_job());
    EXPECT_EQ(server.state(), CbsServer::State::Inactive);

    server.activate(time(0.0));

    EXPECT_EQ(server.state(), CbsServer::State::Ready);
    EXPECT_EQ(server.virtual_time(), time(0.0));
    EXPECT_EQ(server.deadline(), time(10.0));  // now + period
    EXPECT_EQ(server.remaining_budget().count(), 2.0);  // = budget
}

TEST_F(CbsServerTest, StateTransition_ReadyToRunning) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    server.enqueue_job(make_job());
    server.activate(time(0.0));
    EXPECT_EQ(server.state(), CbsServer::State::Ready);

    server.dispatch();

    EXPECT_EQ(server.state(), CbsServer::State::Running);
}

TEST_F(CbsServerTest, StateTransition_RunningToReady_Preempt) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    server.enqueue_job(make_job());
    server.activate(time(0.0));
    server.dispatch();
    EXPECT_EQ(server.state(), CbsServer::State::Running);

    server.preempt();

    EXPECT_EQ(server.state(), CbsServer::State::Ready);
}

TEST_F(CbsServerTest, StateTransition_RunningToInactive_NoMoreJobs) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    server.enqueue_job(make_job());
    server.activate(time(0.0));
    server.dispatch();
    EXPECT_EQ(server.state(), CbsServer::State::Running);

    // Dequeue the completed job first (as EdfScheduler does)
    server.dequeue_job();
    server.complete_job(time(2.0));

    EXPECT_EQ(server.state(), CbsServer::State::Inactive);
}

TEST_F(CbsServerTest, StateTransition_RunningToReady_MoreJobs) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    // Enqueue two jobs
    server.enqueue_job(make_job());
    server.enqueue_job(make_job());
    server.activate(time(0.0));
    server.dispatch();

    // Complete first job (still have second)
    server.dequeue_job();  // Remove completed job
    server.enqueue_job(make_job());  // Add another to have >1

    server.complete_job(time(2.0));

    EXPECT_EQ(server.state(), CbsServer::State::Ready);
}

TEST_F(CbsServerTest, VirtualTimeUpdate_CorrectFormula) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});  // U = 0.2

    server.enqueue_job(make_job());
    server.activate(time(0.0));
    EXPECT_EQ(server.virtual_time(), time(0.0));

    // Update: vt += execution_time / U = 1.0 / 0.2 = 5.0
    server.update_virtual_time(Duration{1.0});

    EXPECT_EQ(server.virtual_time(), time(5.0));
}

TEST_F(CbsServerTest, VirtualTimeUpdate_MultipleUpdates) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});  // U = 0.2

    server.enqueue_job(make_job());
    server.activate(time(0.0));

    server.update_virtual_time(Duration{0.5});  // vt = 0 + 0.5/0.2 = 2.5
    EXPECT_DOUBLE_EQ(server.virtual_time().time_since_epoch().count(), 2.5);

    server.update_virtual_time(Duration{0.3});  // vt = 2.5 + 0.3/0.2 = 4.0
    EXPECT_DOUBLE_EQ(server.virtual_time().time_since_epoch().count(), 4.0);
}

TEST_F(CbsServerTest, BudgetExhaustion_PostponesDeadline) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    server.enqueue_job(make_job());
    server.activate(time(0.0));
    server.dispatch();

    EXPECT_EQ(server.deadline(), time(10.0));
    EXPECT_EQ(server.remaining_budget().count(), 2.0);

    server.exhaust_budget(time(2.0));

    // Deadline postponed: d += T
    EXPECT_EQ(server.deadline(), time(20.0));
    // Budget replenished
    EXPECT_EQ(server.remaining_budget().count(), 2.0);
    // State becomes Ready
    EXPECT_EQ(server.state(), CbsServer::State::Ready);
}

TEST_F(CbsServerTest, BudgetConsumption) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    server.enqueue_job(make_job());
    server.activate(time(0.0));
    EXPECT_EQ(server.remaining_budget().count(), 2.0);

    server.consume_budget(Duration{0.5});
    EXPECT_DOUBLE_EQ(server.remaining_budget().count(), 1.5);

    server.consume_budget(Duration{1.0});
    EXPECT_DOUBLE_EQ(server.remaining_budget().count(), 0.5);

    // Cannot go negative
    server.consume_budget(Duration{1.0});
    EXPECT_DOUBLE_EQ(server.remaining_budget().count(), 0.0);
}

TEST_F(CbsServerTest, PostponeDeadline) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});

    server.enqueue_job(make_job());
    server.activate(time(0.0));
    server.consume_budget(Duration{2.0});
    EXPECT_EQ(server.remaining_budget().count(), 0.0);

    server.postpone_deadline();

    EXPECT_EQ(server.deadline(), time(20.0));
    EXPECT_EQ(server.remaining_budget().count(), 2.0);
}

TEST_F(CbsServerTest, OverrunPolicy_Skip) {
    CbsServer server(0, Duration{2.0}, Duration{10.0}, CbsServer::OverrunPolicy::Skip);

    server.enqueue_job(make_job());
    server.activate(time(0.0));
    server.dispatch();
    EXPECT_EQ(server.job_queue_size(), 1U);

    // New job should be skipped
    server.enqueue_job(make_job());
    EXPECT_EQ(server.job_queue_size(), 1U);  // Still 1
}

TEST_F(CbsServerTest, OverrunPolicy_Abort) {
    CbsServer server(0, Duration{2.0}, Duration{10.0}, CbsServer::OverrunPolicy::Abort);

    server.enqueue_job(make_job(Duration{1.0}));
    server.activate(time(0.0));
    server.dispatch();
    EXPECT_EQ(server.job_queue_size(), 1U);

    // New job should replace current
    server.enqueue_job(make_job(Duration{2.0}));
    EXPECT_EQ(server.job_queue_size(), 1U);
    // The new job is the current one
    EXPECT_EQ(server.current_job()->total_work().count(), 2.0);
}

TEST_F(CbsServerTest, OverrunPolicy_Queue_Default) {
    CbsServer server(0, Duration{2.0}, Duration{10.0});  // Default: Queue

    server.enqueue_job(make_job());
    server.activate(time(0.0));
    server.dispatch();
    EXPECT_EQ(server.job_queue_size(), 1U);

    // New job should be queued
    server.enqueue_job(make_job());
    EXPECT_EQ(server.job_queue_size(), 2U);
}

TEST_F(CbsServerTest, MoveConstructor) {
    CbsServer server1(0, Duration{2.0}, Duration{10.0});
    server1.enqueue_job(make_job());
    server1.activate(time(0.0));

    CbsServer server2(std::move(server1));

    EXPECT_EQ(server2.budget().count(), 2.0);
    EXPECT_EQ(server2.period().count(), 10.0);
    EXPECT_EQ(server2.state(), CbsServer::State::Ready);
    EXPECT_TRUE(server2.has_pending_jobs());
}
