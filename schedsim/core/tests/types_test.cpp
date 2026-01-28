#include <schedsim/core/types.hpp>
#include <gtest/gtest.h>

using namespace schedsim::core;

TEST(TypesTest, DurationArithmetic) {
    Duration d1{5.0};
    Duration d2{3.0};

    EXPECT_DOUBLE_EQ((d1 + d2).count(), 8.0);
    EXPECT_DOUBLE_EQ((d1 - d2).count(), 2.0);
    EXPECT_DOUBLE_EQ((d1 * 2.0).count(), 10.0);
}

TEST(TypesTest, TimePointArithmetic) {
    TimePoint t1{Duration{10.0}};
    Duration d{5.0};

    TimePoint t2 = t1 + d;
    EXPECT_DOUBLE_EQ(t2.time_since_epoch().count(), 15.0);

    Duration diff = t2 - t1;
    EXPECT_DOUBLE_EQ(diff.count(), 5.0);
}

TEST(TypesTest, FrequencyComparison) {
    Frequency f1{1000.0};
    Frequency f2{2000.0};

    EXPECT_LT(f1, f2);
    EXPECT_EQ(f1, Frequency{1000.0});
}

TEST(TypesTest, EnergyAccumulation) {
    Energy e1{10.0};
    Energy e2{5.0};

    e1 += e2;
    EXPECT_DOUBLE_EQ(e1.mj, 15.0);
}
