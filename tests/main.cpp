#include "gtest/gtest.h"
#include "scenario.hpp"

#include <algorithm>
#include <variant>
#include <vector>

TEST(Scenario, Jobs)
{
        EXPECT_EQ(true, true);
}

int main(int argc, char** argv)
{
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
}
