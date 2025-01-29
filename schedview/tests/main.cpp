#include <cstddef>
#include <energy.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <protocols/hardware.hpp>
#include <vector>

class Schedview : public ::testing::Test {};

TEST_F(Schedview, EnergyCpuToCluster)
{
        using namespace protocols::hardware;

        const protocols::hardware::hardware hw{
            {{2, {1.0}, 1.0, {0.1, 0.2, 0.3, 0.4}},
             {2, {1.0}, 1.0, {0.1, 0.2, 0.3, 0.4}},
             {2, {1.0}, 1.0, {0.1, 0.2, 0.3, 0.4}}}};

        EXPECT_EQ(outputs::energy::cpu_to_cluster(hw, 5), 2);
}

int main(int argc, char** argv)
{
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
}
