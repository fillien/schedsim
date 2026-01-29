#include <schedsim/algo/grub_policy.hpp>

#include <schedsim/algo/cbs_server.hpp>
#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class GrubPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("cpu", 1.0);
        auto& cd = engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}}
        });
        proc_ = &engine_.platform().add_processor(pt, cd, pd);
        // Don't finalize here - tests will finalize after adding tasks
    }

    TimePoint time(double seconds) {
        return TimePoint{Duration{seconds}};
    }

    Engine engine_;
    Processor* proc_{nullptr};
};

TEST_F(GrubPolicyTest, InitialActiveUtilization) {
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    GrubPolicy policy(sched);

    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.0);
}

TEST_F(GrubPolicyTest, ActiveUtilizationTracking) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    GrubPolicy policy(sched);

    // Server activated: should add utilization
    policy.on_server_state_change(server, ReclamationPolicy::ServerStateChange::Activated);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.2);  // 2.0/10.0

    // Server dispatched: utilization unchanged
    policy.on_server_state_change(server, ReclamationPolicy::ServerStateChange::Dispatched);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.2);

    // Server completed: utilization removed
    policy.on_server_state_change(server, ReclamationPolicy::ServerStateChange::Completed);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.0);
}

TEST_F(GrubPolicyTest, NonContendingRemovesUtilization) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    GrubPolicy policy(sched);

    policy.on_server_state_change(server, ReclamationPolicy::ServerStateChange::Activated);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.2);

    // NonContending removes utilization (but server still exists)
    policy.on_server_state_change(server, ReclamationPolicy::ServerStateChange::NonContending);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.0);
}

TEST_F(GrubPolicyTest, ComputeVirtualTime_GrubFormula) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    auto& task2 = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{3.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    GrubPolicy policy(sched);

    // Set active utilization to 0.5
    auto& server2 = sched.add_server(task2);
    policy.on_server_state_change(server, ReclamationPolicy::ServerStateChange::Activated);
    policy.on_server_state_change(server2, ReclamationPolicy::ServerStateChange::Activated);

    // active_util = 0.2 + 0.3 = 0.5
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.5);

    // GRUB formula: vt += exec_time / active_util
    // vt = 0 + 1.0 / 0.5 = 2.0
    TimePoint new_vt = policy.compute_virtual_time(server, time(0.0), Duration{1.0});
    EXPECT_DOUBLE_EQ(new_vt.time_since_epoch().count(), 2.0);
}

TEST_F(GrubPolicyTest, ComputeVirtualTime_ClampsMinUtilization) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    GrubPolicy policy(sched);

    // active_util = 0 (no servers active)
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.0);

    // Should clamp to min_utilization (0.01) to avoid division by zero
    // vt = 0 + 1.0 / 0.01 = 100.0
    TimePoint new_vt = policy.compute_virtual_time(server, time(0.0), Duration{1.0});
    EXPECT_DOUBLE_EQ(new_vt.time_since_epoch().count(), 100.0);
}

TEST_F(GrubPolicyTest, EarlyCompletionReturnsTrue) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    GrubPolicy policy(sched);

    // GRUB should return true to enter NonContending state
    bool enter_nc = policy.on_early_completion(server, Duration{1.0});
    EXPECT_TRUE(enter_nc);
}

TEST_F(GrubPolicyTest, BudgetExhaustedReturnsZero) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    GrubPolicy policy(sched);

    // GRUB doesn't grant extra budget
    Duration extra = policy.on_budget_exhausted(server);
    EXPECT_DOUBLE_EQ(extra.count(), 0.0);
}

TEST_F(GrubPolicyTest, MultipleServersUtilization) {
    auto& task1 = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{1.0});
    auto& task2 = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    auto& task3 = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{3.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server1 = sched.add_server(task1);  // U=0.1
    auto& server2 = sched.add_server(task2);  // U=0.2
    auto& server3 = sched.add_server(task3);  // U=0.3

    GrubPolicy policy(sched);

    policy.on_server_state_change(server1, ReclamationPolicy::ServerStateChange::Activated);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.1);

    policy.on_server_state_change(server2, ReclamationPolicy::ServerStateChange::Activated);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.3);

    policy.on_server_state_change(server3, ReclamationPolicy::ServerStateChange::Activated);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.6);

    // Complete one
    policy.on_server_state_change(server2, ReclamationPolicy::ServerStateChange::Completed);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.4);
}
