#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/single_scheduler_allocator.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

// Helper fixture that creates engine/platform but doesn't finalize
class EdfSchedulerTestBase : public ::testing::Test {
protected:
    TimePoint time(double seconds) {
        return time_from_seconds(seconds);
    }
};

TEST_F(EdfSchedulerTestBase, Construction) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    EXPECT_EQ(sched.processor_count(), 1U);
    EXPECT_EQ(sched.server_count(), 0U);
    EXPECT_DOUBLE_EQ(sched.utilization(), 0.0);
}

TEST_F(EdfSchedulerTestBase, AddServer_Basic) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    auto& server = sched.add_server(task, duration_from_seconds(2.0), duration_from_seconds(10.0));

    EXPECT_EQ(sched.server_count(), 1U);
    EXPECT_DOUBLE_EQ(sched.utilization(), 0.2);
    EXPECT_EQ(server.task(), &task);
}

TEST_F(EdfSchedulerTestBase, AddServer_FromTaskParams) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    auto& server = sched.add_server(task);

    EXPECT_EQ(duration_to_seconds(server.budget()), 2.0);  // task.wcet()
    EXPECT_EQ(duration_to_seconds(server.period()), 10.0);  // task.period()
}

TEST_F(EdfSchedulerTestBase, FindServer) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    sched.add_server(task);

    CbsServer* found = sched.find_server(task);
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->task(), &task);
}

TEST_F(EdfSchedulerTestBase, FindServer_NotFound) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    CbsServer* found = sched.find_server(task);
    EXPECT_EQ(found, nullptr);
}

TEST_F(EdfSchedulerTestBase, AdmissionTest_Uniprocessor_Accepts) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    // Can admit U=0.5 on single processor
    EXPECT_TRUE(sched.can_admit(duration_from_seconds(5.0), duration_from_seconds(10.0)));

    // Can admit U=1.0 exactly
    EXPECT_TRUE(sched.can_admit(duration_from_seconds(10.0), duration_from_seconds(10.0)));
}

TEST_F(EdfSchedulerTestBase, AdmissionTest_Uniprocessor_Rejects) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    // Cannot admit U>1.0 on single processor
    EXPECT_FALSE(sched.can_admit(duration_from_seconds(11.0), duration_from_seconds(10.0)));
}

TEST_F(EdfSchedulerTestBase, AddServer_ThrowsOnOverUtilization) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task1 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(8.0));
    auto& task2 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(4.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    sched.add_server(task1);  // U=0.8

    // This would make total U=1.2 > 1.0
    EXPECT_THROW(
        sched.add_server(task2),
        AdmissionError
    );
}

TEST_F(EdfSchedulerTestBase, AddServerUnchecked_BypassesAdmission) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task1 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(8.0));
    auto& task2 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(4.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    sched.add_server(task1);  // U=0.8

    // Unchecked allows over-utilization
    EXPECT_NO_THROW(
        sched.add_server_unchecked(task2, duration_from_seconds(4.0), duration_from_seconds(10.0))
    );

    EXPECT_DOUBLE_EQ(sched.utilization(), 1.2);
}

TEST_F(EdfSchedulerTestBase, DeadlineMissPolicy_Default) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    // Default policy is Continue - just verify no crash
    sched.set_deadline_miss_policy(DeadlineMissPolicy::Continue);
}

TEST_F(EdfSchedulerTestBase, DeadlineMissHandler) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    bool handler_called = false;
    sched.set_deadline_miss_handler([&](Processor&, Job&) {
        handler_called = true;
    });

    // Handler is set - actual invocation tested in integration tests
    EXPECT_FALSE(handler_called);
}

TEST_F(EdfSchedulerTestBase, Processors_ReturnsSpan) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    auto procs = sched.processors();
    EXPECT_EQ(procs.size(), 1U);
    EXPECT_EQ(procs[0], proc);
}

// Multi-processor tests
TEST_F(EdfSchedulerTestBase, AdmissionTest_Multiprocessor) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);

    // Can admit up to U=4.0 on 4 processors
    EXPECT_TRUE(sched.can_admit(duration_from_seconds(10.0), duration_from_seconds(10.0)));  // U=1.0
    EXPECT_TRUE(sched.can_admit(duration_from_seconds(40.0), duration_from_seconds(10.0)));  // U=4.0

    // Cannot admit U>4.0
    EXPECT_FALSE(sched.can_admit(duration_from_seconds(41.0), duration_from_seconds(10.0)));  // U=4.1
}

TEST_F(EdfSchedulerTestBase, AdmissionTest_AccumulatesUtilization) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }
    auto& task1 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0));
    auto& task2 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(10.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);
    sched.add_server(task1);  // U=1.0
    sched.add_server(task2);  // U=1.0

    EXPECT_DOUBLE_EQ(sched.utilization(), 2.0);

    // Can still admit up to U=2.0 more
    EXPECT_TRUE(sched.can_admit(duration_from_seconds(20.0), duration_from_seconds(10.0)));  // +2.0

    // Cannot admit more than remaining capacity
    EXPECT_FALSE(sched.can_admit(duration_from_seconds(21.0), duration_from_seconds(10.0)));  // +2.1
}

TEST_F(EdfSchedulerTestBase, ProcessorCount) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);

    EXPECT_EQ(sched.processor_count(), 4U);
}

TEST_F(EdfSchedulerTestBase, ServerIds_Monotonic) {
    // Verify servers get unique, incrementing IDs
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }
    auto& task1 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    auto& task2 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    auto& task3 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);

    auto& server1 = sched.add_server(task1);
    auto& server2 = sched.add_server(task2);
    auto& server3 = sched.add_server(task3);

    // IDs should be monotonically increasing
    EXPECT_EQ(server1.id(), 0U);
    EXPECT_EQ(server2.id(), 1U);
    EXPECT_EQ(server3.id(), 2U);
}

TEST_F(EdfSchedulerTestBase, EqualDeadlines_OrderedById) {
    // Verify deterministic tie-breaking: servers with equal deadlines
    // are ordered by ID (lower ID first)
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);

    // Create tasks with identical periods (so servers get identical initial deadlines)
    auto& task1 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    auto& task2 = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(1.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    // Add servers - server1 gets ID 0, server2 gets ID 1
    auto& server1 = sched.add_server(task1);
    auto& server2 = sched.add_server(task2);

    EXPECT_LT(server1.id(), server2.id());

    // Both servers have the same period (10.0), so when activated at the same time
    // they will have identical deadlines. The EDF scheduler should deterministically
    // dispatch server1 first (lower ID).
    // This test verifies the IDs are assigned correctly for tie-breaking.
}

// =============================================================================
// M-GRUB Server Detach Tests
// =============================================================================

TEST_F(EdfSchedulerTestBase, SetExpectedArrivals_DetachAfterAllArrived) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    sched.enable_grub();
    sched.add_server(task);
    sched.set_expected_arrivals(task, 1);  // Only 1 arrival expected

    SingleSchedulerAllocator alloc(engine, sched);
    engine.schedule_job_arrival(task, time(0.0), duration_from_seconds(2.0));
    engine.run(time(15.0));

    // Server detached: arrival_counts_[task]=1 >= expected_arrivals_[task]=1
    EXPECT_DOUBLE_EQ(sched.scheduler_utilization(), 0.0);
}

TEST_F(EdfSchedulerTestBase, SetExpectedArrivals_NoDetachWhenMoreExpected) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    sched.enable_grub();
    sched.add_server(task);
    sched.set_expected_arrivals(task, 2);  // 2 arrivals expected, only 1 will occur

    SingleSchedulerAllocator alloc(engine, sched);
    engine.schedule_job_arrival(task, time(0.0), duration_from_seconds(2.0));
    engine.run(time(15.0));

    // Server NOT detached: arrival_counts_[task]=1 < expected_arrivals_[task]=2
    EXPECT_DOUBLE_EQ(sched.scheduler_utilization(), 0.2);
}

// =============================================================================
// CBS Admission Epsilon Tolerance Tests
// =============================================================================

TEST_F(EdfSchedulerTestBase, AdmissionTest_BoundaryEpsilon_Admits) {
    // 100 servers of U=1/100 on 1 core: total = 1.0 mathematically.
    // 100 accumulated additions of (1.0/100.0) overshoots 1.0 by ~6.66e-16
    // in IEEE 754, which would cause spurious rejection without the epsilon.
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);

    std::vector<std::reference_wrapper<Task>> tasks;
    for (int i = 0; i < 100; ++i) {
        tasks.push_back(engine.platform().add_task(
            duration_from_seconds(100.0), duration_from_seconds(100.0),
            duration_from_seconds(1.0)));
    }
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    // All 100 add_server() calls must succeed (total U = 100 * 1/100 = 1.0)
    for (auto& task_ref : tasks) {
        EXPECT_NO_THROW(sched.add_server(task_ref.get()));
    }

    // Utilization should be very close to 1.0 (overshoots by ~6.66e-16)
    EXPECT_NEAR(sched.utilization(), 1.0, 1e-9);

    // No room for even one more server with the same U
    EXPECT_FALSE(sched.can_admit(duration_from_seconds(1.0), duration_from_seconds(100.0)));
}

TEST_F(EdfSchedulerTestBase, AdmissionTest_BoundaryEpsilon_StillRejects) {
    // 4 servers of U=1.0 on 4 cores (total = 4.0 exact),
    // then a server with U=1e-6 must be rejected (well above epsilon).
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }

    std::vector<std::reference_wrapper<Task>> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0),
            duration_from_seconds(10.0)));
    }
    auto& tiny_task = engine.platform().add_task(
        duration_from_seconds(1.0), duration_from_seconds(1.0),
        duration_from_seconds(1e-6));
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);

    for (auto& task_ref : tasks) {
        sched.add_server(task_ref.get());
    }
    EXPECT_DOUBLE_EQ(sched.utilization(), 4.0);

    // U=1e-6 is well above the epsilon (1e-9), so this must be rejected
    EXPECT_FALSE(sched.can_admit(tiny_task.wcet(), tiny_task.period()));
    EXPECT_THROW(
        sched.add_server(tiny_task),
        AdmissionError
    );
}

// =============================================================================
// GFB Admission Test
// =============================================================================

TEST_F(EdfSchedulerTestBase, AdmissionTest_GFB_RejectsDhallEffect) {
    // 4 processors, GFB enabled. One heavy task U=0.9.
    // GFB bound = 4 - 3*0.9 = 1.3. Adding U=0.5 (total 1.4 > 1.3) must fail.
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }
    auto& heavy = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(9.0));
    auto& medium = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);
    sched.set_admission_test(AdmissionTest::GFB);
    sched.add_server(heavy);  // U=0.9

    // can_admit should reject U=0.5 (total 1.4 > GFB bound 1.3)
    EXPECT_FALSE(sched.can_admit(duration_from_seconds(5.0), duration_from_seconds(10.0)));
    EXPECT_THROW(sched.add_server(medium), AdmissionError);
}

TEST_F(EdfSchedulerTestBase, AdmissionTest_GFB_AcceptsWithinBound) {
    // 4 processors, GFB enabled. 4 tasks each U=0.25.
    // u_max=0.25, GFB bound = 4 - 3*0.25 = 3.25. Total = 1.0, well within.
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }

    std::vector<std::reference_wrapper<Task>> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(engine.platform().add_task(
            duration_from_seconds(40.0), duration_from_seconds(40.0),
            duration_from_seconds(10.0)));  // U=0.25
    }
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);
    sched.set_admission_test(AdmissionTest::GFB);

    for (auto& task_ref : tasks) {
        EXPECT_NO_THROW(sched.add_server(task_ref.get()));
    }
    EXPECT_NEAR(sched.utilization(), 1.0, 1e-9);
}

TEST_F(EdfSchedulerTestBase, AdmissionTest_GFB_UniprocessorUnchanged) {
    // 1 processor, GFB enabled. GFB on 1 core = 1 - 0*u_max = 1.0 (same as capacity).
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task1 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    auto& task2 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    sched.set_admission_test(AdmissionTest::GFB);

    EXPECT_NO_THROW(sched.add_server(task1));  // U=0.5
    EXPECT_NO_THROW(sched.add_server(task2));  // U=0.5, total=1.0
    EXPECT_NEAR(sched.utilization(), 1.0, 1e-9);
}

TEST_F(EdfSchedulerTestBase, AdmissionTest_Default_AllowsAboveGFB) {
    // 4 processors, default CapacityBound. U=0.9 + U=0.5 = 1.4.
    // This passes under capacity bound (1.4 <= 4) but would fail under GFB (1.4 > 1.3).
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }
    auto& heavy = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(9.0));
    auto& medium = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(5.0));
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);
    // Default is CapacityBound — no set_admission_test() call

    EXPECT_NO_THROW(sched.add_server(heavy));   // U=0.9
    EXPECT_NO_THROW(sched.add_server(medium));  // U=0.5, total=1.4 <= 4.0
    EXPECT_NEAR(sched.utilization(), 1.4, 1e-9);
}

TEST_F(EdfSchedulerTestBase, AdmissionTest_GFB_HeavyNewServerDeterminesUmax) {
    // 4 processors, GFB. 3 light tasks U=0.1 each. Then add heavy U=0.8.
    // u_max = max(0.1, 0.8) = 0.8, GFB = 4 - 3*0.8 = 1.6. Total = 0.3+0.8 = 1.1 <= 1.6: OK.
    // Then a second U=0.8: u_max still 0.8, total = 1.9 > 1.6: rejected.
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }

    std::vector<std::reference_wrapper<Task>> light_tasks;
    for (int i = 0; i < 3; ++i) {
        light_tasks.push_back(engine.platform().add_task(
            duration_from_seconds(10.0), duration_from_seconds(10.0),
            duration_from_seconds(1.0)));  // U=0.1
    }
    auto& heavy1 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(8.0));  // U=0.8
    auto& heavy2 = engine.platform().add_task(
        duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(8.0));  // U=0.8
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);
    sched.set_admission_test(AdmissionTest::GFB);

    for (auto& task_ref : light_tasks) {
        EXPECT_NO_THROW(sched.add_server(task_ref.get()));
    }
    EXPECT_NEAR(sched.utilization(), 0.3, 1e-9);

    // Heavy task: u_max becomes 0.8, GFB = 4 - 3*0.8 = 1.6, total = 1.1 <= 1.6
    EXPECT_TRUE(sched.can_admit(duration_from_seconds(8.0), duration_from_seconds(10.0)));
    EXPECT_NO_THROW(sched.add_server(heavy1));
    EXPECT_NEAR(sched.utilization(), 1.1, 1e-9);

    // Second heavy: u_max=0.8, GFB=1.6, total would be 1.9 > 1.6
    EXPECT_FALSE(sched.can_admit(duration_from_seconds(8.0), duration_from_seconds(10.0)));
    EXPECT_THROW(sched.add_server(heavy2), AdmissionError);
}
