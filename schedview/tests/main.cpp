#include "protocols/hardware.hpp"
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

class Schedview : public ::testing::Test {};

auto cpu_to_cluster(const protocols::hardware::hardware& hw, const std::size_t cpu) -> std::size_t
{
        std::size_t min_cluster{0};
        std::size_t index{hw.clusters.at(min_cluster).nb_procs};
        while (cpu > index) {
                min_cluster += 1;
                index += hw.clusters.at(min_cluster).nb_procs;
        }

        return min_cluster;
}

TEST_F(Schedview, EnergyCpuToCluster)
{
        using namespace protocols::hardware;

        const protocols::hardware::hardware hw {{
                {2, {1.0}, 1.0, {0.1, 0.2, 0.3, 0.4}},
                {2, {1.0}, 1.0, {0.1, 0.2, 0.3, 0.4}},
                {2, {1.0}, 1.0, {0.1, 0.2, 0.3, 0.4}}
        }};

        EXPECT_EQ(cpu_to_cluster(hw, 5), 2);
}

int main(int argc, char** argv)
{
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
}
