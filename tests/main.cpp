#include "../src/engine.hpp"
#include "../src/event.hpp"
#include "../src/scheduler.hpp"
#include "../src/server.hpp"
#include "../src/task.hpp"
#include "gtest/gtest.h"

TEST(SchedulerTest, GetPriority)
{
        EXPECT_EQ(get_priority(events::job_finished{}), 0);
        EXPECT_EQ(get_priority(events::serv_budget_exhausted{}), 1);
        EXPECT_EQ(get_priority(events::job_arrival{}), 2);
        EXPECT_EQ(get_priority(events::serv_inactive{}), 3);
        EXPECT_EQ(get_priority(events::serv_running{}), 100);
}

TEST(TaskServer, Association)
{
        auto sim = std::make_shared<engine>();

        auto task1 = std::make_shared<task>(sim, 1, 0, 0);
        EXPECT_EQ(task1->has_server(), false);

        auto server1 = std::make_shared<server>(sim);
        task1->set_server(server1);
        EXPECT_EQ(task1->has_server(), true);
        EXPECT_EQ(server1->has_task(), true);

        // EXPECT_EQ(server1->get_task(), task1);
        // EXPECT_EQ(task1->get_server(), server1);
}

int main(int argc, char** argv)
{
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
}
