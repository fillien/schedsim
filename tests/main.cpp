#include "../src/engine.hpp"
#include "../src/event.hpp"
#include "../src/scheduler.hpp"
#include "../src/server.hpp"
#include "../src/task.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <variant>
#include <vector>

TEST(EventHandling, CheckHandlingOrder)
{
        using namespace events;

        std::vector<event> evts{
            job_arrival{}, job_finished{}, serv_inactive{}, serv_budget_exhausted{}};

        std::sort(std::begin(evts), std::end(evts), compare_events);

        EXPECT_EQ(std::holds_alternative<job_finished>(evts.at(0)), true);
        EXPECT_EQ(std::holds_alternative<serv_budget_exhausted>(evts.at(1)), true);
        EXPECT_EQ(std::holds_alternative<serv_inactive>(evts.at(2)), true);
        EXPECT_EQ(std::holds_alternative<job_arrival>(evts.at(3)), true);
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
