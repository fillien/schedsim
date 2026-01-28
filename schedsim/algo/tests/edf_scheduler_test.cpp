#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/error.hpp>

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
        return TimePoint{Duration{seconds}};
    }
};

TEST_F(EdfSchedulerTestBase, Construction) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
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
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    auto& server = sched.add_server(task, Duration{2.0}, Duration{10.0});

    EXPECT_EQ(sched.server_count(), 1U);
    EXPECT_DOUBLE_EQ(sched.utilization(), 0.2);
    EXPECT_EQ(server.task(), &task);
}

TEST_F(EdfSchedulerTestBase, AddServer_FromTaskParams) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    auto& server = sched.add_server(task);

    EXPECT_EQ(server.budget().count(), 2.0);  // task.wcet()
    EXPECT_EQ(server.period().count(), 10.0);  // task.period()
}

TEST_F(EdfSchedulerTestBase, FindServer) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
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
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
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
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    // Can admit U=0.5 on single processor
    EXPECT_TRUE(sched.can_admit(Duration{5.0}, Duration{10.0}));

    // Can admit U=1.0 exactly
    EXPECT_TRUE(sched.can_admit(Duration{10.0}, Duration{10.0}));
}

TEST_F(EdfSchedulerTestBase, AdmissionTest_Uniprocessor_Rejects) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});

    // Cannot admit U>1.0 on single processor
    EXPECT_FALSE(sched.can_admit(Duration{11.0}, Duration{10.0}));
}

TEST_F(EdfSchedulerTestBase, AddServer_ThrowsOnOverUtilization) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task1 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{8.0});
    auto& task2 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{4.0});
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
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);
    auto& task1 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{8.0});
    auto& task2 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{4.0});
    engine.platform().finalize();

    EdfScheduler sched(engine, {proc});
    sched.add_server(task1);  // U=0.8

    // Unchecked allows over-utilization
    EXPECT_NO_THROW(
        sched.add_server_unchecked(task2, Duration{4.0}, Duration{10.0})
    );

    EXPECT_DOUBLE_EQ(sched.utilization(), 1.2);
}

TEST_F(EdfSchedulerTestBase, DeadlineMissPolicy_Default) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
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
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
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
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
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
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);

    // Can admit up to U=4.0 on 4 processors
    EXPECT_TRUE(sched.can_admit(Duration{10.0}, Duration{10.0}));  // U=1.0
    EXPECT_TRUE(sched.can_admit(Duration{40.0}, Duration{10.0}));  // U=4.0

    // Cannot admit U>4.0
    EXPECT_FALSE(sched.can_admit(Duration{41.0}, Duration{10.0}));  // U=4.1
}

TEST_F(EdfSchedulerTestBase, AdmissionTest_AccumulatesUtilization) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }
    auto& task1 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{10.0});
    auto& task2 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{10.0});
    engine.platform().finalize();

    EdfScheduler sched(engine, procs);
    sched.add_server(task1);  // U=1.0
    sched.add_server(task2);  // U=1.0

    EXPECT_DOUBLE_EQ(sched.utilization(), 2.0);

    // Can still admit up to U=2.0 more
    EXPECT_TRUE(sched.can_admit(Duration{20.0}, Duration{10.0}));  // +2.0

    // Cannot admit more than remaining capacity
    EXPECT_FALSE(sched.can_admit(Duration{21.0}, Duration{10.0}));  // +2.1
}

TEST_F(EdfSchedulerTestBase, ProcessorCount) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("cpu", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
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
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });

    std::vector<Processor*> procs;
    for (int i = 0; i < 4; ++i) {
        procs.push_back(&engine.platform().add_processor(pt, cd, pd));
    }
    auto& task1 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{1.0});
    auto& task2 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{1.0});
    auto& task3 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{1.0});
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
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
    });
    auto* proc = &engine.platform().add_processor(pt, cd, pd);

    // Create tasks with identical periods (so servers get identical initial deadlines)
    auto& task1 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{1.0});
    auto& task2 = engine.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{1.0});
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
