#include <schedsim/core/power_domain.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

TEST(PowerDomainTest, Construction) {
    std::vector<CStateLevel> c_states = {
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
        {1, CStateScope::PerProcessor, duration_from_seconds(0.001), Power{50.0}},
        {2, CStateScope::DomainWide, duration_from_seconds(0.01), Power{10.0}},
    };

    PowerDomain pd(0, c_states);

    EXPECT_EQ(pd.id(), 0U);
    EXPECT_EQ(pd.c_states().size(), 3U);
}

TEST(PowerDomainTest, CStateLevels) {
    std::vector<CStateLevel> c_states = {
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
        {1, CStateScope::PerProcessor, duration_from_seconds(0.001), Power{50.0}},
    };

    PowerDomain pd(0, c_states);

    auto states = pd.c_states();
    EXPECT_EQ(states[0].level, 0);
    EXPECT_EQ(states[0].scope, CStateScope::PerProcessor);
    EXPECT_EQ(duration_to_seconds(states[0].wake_latency), 0.0);
    EXPECT_EQ(states[0].power.mw, 100.0);

    EXPECT_EQ(states[1].level, 1);
    EXPECT_EQ(states[1].scope, CStateScope::PerProcessor);
    EXPECT_EQ(duration_to_seconds(states[1].wake_latency), 0.001);
    EXPECT_EQ(states[1].power.mw, 50.0);
}

TEST(PowerDomainTest, CStateScope) {
    std::vector<CStateLevel> c_states = {
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
        {1, CStateScope::DomainWide, duration_from_seconds(0.01), Power{10.0}},
    };

    PowerDomain pd(0, c_states);

    auto states = pd.c_states();
    EXPECT_EQ(states[0].scope, CStateScope::PerProcessor);
    EXPECT_EQ(states[1].scope, CStateScope::DomainWide);
}

TEST(PowerDomainTest, ProcessorsInitiallyEmpty) {
    std::vector<CStateLevel> c_states = {
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
    };

    PowerDomain pd(0, c_states);

    EXPECT_TRUE(pd.processors().empty());
}

TEST(PowerDomainTest, MoveConstruction) {
    std::vector<CStateLevel> c_states = {
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
        {1, CStateScope::PerProcessor, duration_from_seconds(0.001), Power{50.0}},
    };

    PowerDomain pd1(0, c_states);
    PowerDomain pd2(std::move(pd1));

    EXPECT_EQ(pd2.id(), 0U);
    EXPECT_EQ(pd2.c_states().size(), 2U);
}

TEST(PowerDomainTest, EmptyCStates) {
    std::vector<CStateLevel> c_states;
    PowerDomain pd(0, c_states);

    EXPECT_TRUE(pd.c_states().empty());
}
