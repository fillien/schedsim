#include "engine.hpp"
#include "platform.hpp"
#include "processor.hpp"
#include "server.hpp"
#include "task.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

class Schedsim : public ::testing::Test {};

TEST_F(Schedsim, ProcessorGetterId)
{
        auto sim = std::make_shared<Engine>(false);
        auto plat = std::make_shared<platform>(sim, false);
        sim->set_platform(plat);

        plat->clusters.push_back(std::make_shared<Cluster>(sim, 1, std::vector<double>{1.0}, 1.0));

        EXPECT_EQ(plat->clusters.at(0)->processors.at(0)->get_id(), 0);
}

TEST_F(Schedsim, ProcessorOrder)
{
        std::size_t NB_PROCS{4};
        double EFF_FREQ{1};
        std::vector<double> freqs = {EFF_FREQ};
        bool FREESCALING{false};

        auto sim = std::make_shared<Engine>(false);

        auto plat = std::make_shared<platform>(sim, FREESCALING);
        sim->set_platform(plat);
        plat->clusters.push_back(std::make_shared<Cluster>(sim, 1, freqs, EFF_FREQ));
        plat->clusters.back()->create_procs(NB_PROCS);

        std::shared_ptr<Scheduler> sched = std::make_shared<sched::Parallel>(sim);
        sim->set_scheduler(sched);

        auto s0 = std::make_shared<server>(sim);
        s0->current_state = server::state::running;
        s0->relative_deadline = 1;
        auto s1 = std::make_shared<server>(sim);
        s0->current_state = server::state::running;
        s0->relative_deadline = 1;

        auto t0 = std::make_shared<Task>(sim, 0, 1, 0.1);
        t0->set_server(s0);
        auto t1 = std::make_shared<Task>(sim, 1, 1, 0.1);
        t1->set_server(s1);

        auto p_idle = plat->clusters.at(0)->processors.at(0);
        p_idle->change_state(processor::state::idle);

        auto p_run = plat->clusters.at(0)->processors.at(1);
        p_run->change_state(processor::state::running);
        p_run->set_task(s0->get_task());
        // p_run->set_server(s0);

        auto p_sleep = plat->clusters.at(0)->processors.at(2);
        p_sleep->change_state(processor::state::sleep);

        auto p_change = plat->clusters.at(0)->processors.at(3);
        p_change->change_state(processor::state::change);

        EXPECT_TRUE(sched::Parallel::processor_order(*p_idle, *p_idle));
        EXPECT_TRUE(sched::Parallel::processor_order(*p_idle, *p_run));
        EXPECT_TRUE(sched::Parallel::processor_order(*p_idle, *p_sleep));
        EXPECT_FALSE(sched::Parallel::processor_order(*p_run, *p_idle));
        EXPECT_TRUE(sched::Parallel::processor_order(*p_run, *p_run));
        EXPECT_TRUE(sched::Parallel::processor_order(*p_run, *p_sleep));
        EXPECT_FALSE(sched::Parallel::processor_order(*p_sleep, *p_idle));
        EXPECT_FALSE(sched::Parallel::processor_order(*p_sleep, *p_run));
        EXPECT_FALSE(sched::Parallel::processor_order(*p_sleep, *p_sleep));

        EXPECT_FALSE(sched::Parallel::processor_order(*p_change, *p_sleep));
        EXPECT_FALSE(sched::Parallel::processor_order(*p_change, *p_run));
        EXPECT_FALSE(sched::Parallel::processor_order(*p_change, *p_idle));
        EXPECT_FALSE(sched::Parallel::processor_order(*p_change, *p_change));
        EXPECT_FALSE(sched::Parallel::processor_order(*p_sleep, *p_change));
        EXPECT_TRUE(sched::Parallel::processor_order(*p_run, *p_change));
        EXPECT_TRUE(sched::Parallel::processor_order(*p_idle, *p_change));
}

int main(int argc, char** argv)
{
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
}
