#include <schedsim/algo/cluster.hpp>

#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo;
using namespace schedsim::core;

class ClusterTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& pt = engine_.platform().add_processor_type("cpu", 1.5);
        cd_ = &engine_.platform().add_clock_domain(Frequency{200.0}, Frequency{2000.0});
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, Duration{0.0}, Power{100.0}},
            {1, CStateScope::PerProcessor, Duration{0.001}, Power{10.0}}
        });
        for (int i = 0; i < 4; ++i) {
            procs_[i] = &engine_.platform().add_processor(pt, *cd_, pd);
        }
        engine_.platform().finalize();

        proc_vec_.assign(procs_, procs_ + 4);
        sched_ = std::make_unique<EdfScheduler>(engine_, proc_vec_);
    }

    Engine engine_;
    ClockDomain* cd_{nullptr};
    Processor* procs_[4]{};
    std::vector<Processor*> proc_vec_;
    std::unique_ptr<EdfScheduler> sched_;
};

TEST_F(ClusterTest, Perf_ReturnsConstructorValue) {
    Cluster cluster(*cd_, *sched_, 1.5, 2000.0);
    EXPECT_DOUBLE_EQ(cluster.perf(), 1.5);
}

TEST_F(ClusterTest, ScaleSpeed_SameCluster_IsOne) {
    // reference_freq_max == this.freq_max => scale_speed = 1.0
    Cluster cluster(*cd_, *sched_, 1.0, 2000.0);
    EXPECT_DOUBLE_EQ(cluster.scale_speed(), 1.0);
}

TEST_F(ClusterTest, ScaleSpeed_CrossClusterNormalization) {
    // Simulate a LITTLE cluster with freq_max=1000, reference (big) freq_max=2000
    // scale_speed = 2000/1000 = 2.0
    Engine engine2;
    auto& pt2 = engine2.platform().add_processor_type("little", 0.5);
    auto& cd2 = engine2.platform().add_clock_domain(Frequency{100.0}, Frequency{1000.0});
    auto& pd2 = engine2.platform().add_power_domain({
        {0, CStateScope::PerProcessor, Duration{0.0}, Power{50.0}}
    });
    auto& proc2 = engine2.platform().add_processor(pt2, cd2, pd2);
    engine2.platform().finalize();

    EdfScheduler sched2(engine2, {&proc2});

    Cluster little_cluster(cd2, sched2, 0.5, 2000.0);
    EXPECT_DOUBLE_EQ(little_cluster.scale_speed(), 2.0);
}

TEST_F(ClusterTest, ScaledUtilization_Computation) {
    // scale_speed = 2000/2000 = 1.0, perf = 1.5
    // scaled_util = 0.6 * 1.0 / 1.5 = 0.4
    Cluster cluster(*cd_, *sched_, 1.5, 2000.0);
    EXPECT_DOUBLE_EQ(cluster.scaled_utilization(0.6), 0.4);
}

TEST_F(ClusterTest, UTarget_DefaultOne) {
    Cluster cluster(*cd_, *sched_, 1.0, 2000.0);
    EXPECT_DOUBLE_EQ(cluster.u_target(), 1.0);
}

TEST_F(ClusterTest, UTarget_Mutable) {
    Cluster cluster(*cd_, *sched_, 1.0, 2000.0);
    cluster.set_u_target(0.75);
    EXPECT_DOUBLE_EQ(cluster.u_target(), 0.75);
}

TEST_F(ClusterTest, ProcessorCount_DelegatesToClockDomain) {
    Cluster cluster(*cd_, *sched_, 1.0, 2000.0);
    EXPECT_EQ(cluster.processor_count(), 4u);
}

TEST_F(ClusterTest, Utilization_DelegatesToScheduler) {
    Cluster cluster(*cd_, *sched_, 1.0, 2000.0);
    // No servers added, utilization = 0
    EXPECT_DOUBLE_EQ(cluster.utilization(), 0.0);
}

TEST_F(ClusterTest, CanAdmit_DelegatesToScheduler) {
    Cluster cluster(*cd_, *sched_, 1.0, 2000.0);
    // 4 processors, can admit utilization 0.5 easily
    EXPECT_TRUE(cluster.can_admit(Duration{1.0}, Duration{2.0}));
}

TEST_F(ClusterTest, ClockDomain_Accessors) {
    Cluster cluster(*cd_, *sched_, 1.0, 2000.0);
    EXPECT_EQ(&cluster.clock_domain(), cd_);
    EXPECT_EQ(&cluster.scheduler(), sched_.get());

    const Cluster& cref = cluster;
    EXPECT_EQ(&cref.clock_domain(), cd_);
    EXPECT_EQ(&cref.scheduler(), sched_.get());
}
