#include <schedsim/core/processor_type.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

TEST(ProcessorTypeTest, Construction) {
    ProcessorType pt(0, "big", 1.5);

    EXPECT_EQ(pt.id(), 0U);
    EXPECT_EQ(pt.name(), "big");
    EXPECT_DOUBLE_EQ(pt.performance(), 1.5);
}

TEST(ProcessorTypeTest, DifferentIds) {
    ProcessorType pt1(0, "big", 1.5);
    ProcessorType pt2(1, "LITTLE", 0.5);

    EXPECT_EQ(pt1.id(), 0U);
    EXPECT_EQ(pt2.id(), 1U);
}

TEST(ProcessorTypeTest, PerformanceValues) {
    ProcessorType fast(0, "fast", 2.0);
    ProcessorType slow(1, "slow", 0.25);

    EXPECT_DOUBLE_EQ(fast.performance(), 2.0);
    EXPECT_DOUBLE_EQ(slow.performance(), 0.25);
}

TEST(ProcessorTypeTest, MoveConstruction) {
    ProcessorType pt1(0, "big", 1.5);
    ProcessorType pt2(std::move(pt1));

    EXPECT_EQ(pt2.id(), 0U);
    EXPECT_EQ(pt2.name(), "big");
    EXPECT_DOUBLE_EQ(pt2.performance(), 1.5);
}

TEST(ProcessorTypeTest, MoveAssignment) {
    ProcessorType pt1(0, "big", 1.5);
    ProcessorType pt2(1, "LITTLE", 0.5);

    pt2 = std::move(pt1);

    EXPECT_EQ(pt2.id(), 0U);
    EXPECT_EQ(pt2.name(), "big");
    EXPECT_DOUBLE_EQ(pt2.performance(), 1.5);
}
