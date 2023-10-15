#include "../src/event.hpp"
#include "../src/scheduler.hpp"
#include "gtest/gtest.h"

TEST(SchedulerTest, GetPriority) {
        EXPECT_EQ(get_priority(events::job_finished{}), 0);
        EXPECT_EQ(get_priority(events::serv_budget_exhausted{}), 1);
        EXPECT_EQ(get_priority(events::job_arrival{}), 2);
        EXPECT_EQ(get_priority(events::serv_inactive{}), 3);
        EXPECT_EQ(get_priority(events::serv_running{}), 100);
}

int main(int argc, char** argv) {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
}
