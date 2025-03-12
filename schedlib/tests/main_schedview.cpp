#include <cstddef>
#include <energy.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <protocols/hardware.hpp>
#include <vector>

class Schedview : public ::testing::Test {};

TEST_F(Schedview, EnergyCpuToCluster) { EXPECT_EQ(true, true); }

int main(int argc, char** argv)
{
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
}
