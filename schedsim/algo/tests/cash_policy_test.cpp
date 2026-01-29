#include <schedsim/algo/cash_policy.hpp>

#include <schedsim/algo/cbs_server.hpp>
#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class CashPolicyTest : public ::testing::Test {
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

TEST_F(CashPolicyTest, InitialState) {
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    CashPolicy policy(sched);

    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.0);
    EXPECT_DOUBLE_EQ(policy.spare_budget().count(), 0.0);
}

TEST_F(CashPolicyTest, EarlyCompletionAddsSpareBudget) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    CashPolicy policy(sched);

    // Early completion with 1.0 remaining budget
    bool enter_nc = policy.on_early_completion(server, Duration{1.0});

    // CASH doesn't use NonContending
    EXPECT_FALSE(enter_nc);
    // Spare budget should be accumulated
    EXPECT_DOUBLE_EQ(policy.spare_budget().count(), 1.0);
}

TEST_F(CashPolicyTest, SpareAccumulation) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    CashPolicy policy(sched);

    policy.on_early_completion(server, Duration{0.5});
    EXPECT_DOUBLE_EQ(policy.spare_budget().count(), 0.5);

    policy.on_early_completion(server, Duration{0.3});
    EXPECT_DOUBLE_EQ(policy.spare_budget().count(), 0.8);

    policy.on_early_completion(server, Duration{0.2});
    EXPECT_DOUBLE_EQ(policy.spare_budget().count(), 1.0);
}

TEST_F(CashPolicyTest, BudgetExhaustedGrantsSpareBudget) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    CashPolicy policy(sched);

    // Add some spare budget
    policy.on_early_completion(server, Duration{1.5});
    EXPECT_DOUBLE_EQ(policy.spare_budget().count(), 1.5);

    // Budget exhausted: should get all spare budget
    Duration granted = policy.on_budget_exhausted(server);
    EXPECT_DOUBLE_EQ(granted.count(), 1.5);
    EXPECT_DOUBLE_EQ(policy.spare_budget().count(), 0.0);
}

TEST_F(CashPolicyTest, BudgetExhaustedNoSpare) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    CashPolicy policy(sched);

    // No spare budget
    Duration granted = policy.on_budget_exhausted(server);
    EXPECT_DOUBLE_EQ(granted.count(), 0.0);
}

TEST_F(CashPolicyTest, ActiveUtilizationTracking) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    CashPolicy policy(sched);

    policy.on_server_state_change(server, ReclamationPolicy::ServerStateChange::Activated);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.2);

    policy.on_server_state_change(server, ReclamationPolicy::ServerStateChange::Completed);
    EXPECT_DOUBLE_EQ(policy.active_utilization(), 0.0);
}

TEST_F(CashPolicyTest, UsesDefaultVirtualTimeFormula) {
    auto& task = engine_.platform().add_task(Duration{10.0}, Duration{10.0}, Duration{2.0});
    engine_.platform().finalize();

    EdfScheduler sched(engine_, {proc_});
    auto& server = sched.add_server(task);

    CashPolicy policy(sched);

    // CASH uses default CBS formula: vt += exec_time / U_server
    // U_server = 0.2, exec_time = 1.0
    // vt = 0 + 1.0 / 0.2 = 5.0
    TimePoint new_vt = policy.compute_virtual_time(server, time(0.0), Duration{1.0});
    EXPECT_DOUBLE_EQ(new_vt.time_since_epoch().count(), 5.0);
}
