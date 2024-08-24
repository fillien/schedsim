#include "engine.hpp"
#include "platform.hpp"
#include "processor.hpp"
#include "schedulers/parallel.hpp"
#include "server.hpp"
#include "task.hpp"
#include <gtest/gtest.h>
#include <memory>

class Schedsim : public ::testing::Test {};

TEST_F(Schedsim, ProcessorGetterId)
{
        auto sim = std::make_shared<engine>();
        processor p1{sim, 5};

        EXPECT_EQ(p1.get_id(), 5);
}

TEST_F(Schedsim, ProcessorOrder)
{
        std::size_t NB_PROCS{4};
        double EFF_FREQ{1};
        std::vector<double> freqs = {EFF_FREQ};
        bool FREESCALING{false};

        auto sim = std::make_shared<engine>();

        auto plat = std::make_shared<platform>(sim, NB_PROCS, freqs, EFF_FREQ, FREESCALING);
        sim->set_platform(plat);

        std::shared_ptr<scheduler> sched = std::make_shared<sched_parallel>(sim);
        sim->set_scheduler(sched);

        auto s0 = std::make_shared<server>(sim);
        s0->current_state = server::state::running;
        s0->relative_deadline = 1;
        auto s1 = std::make_shared<server>(sim);
        s0->current_state = server::state::running;
        s0->relative_deadline = 1;

        auto t0 = std::make_shared<task>(sim, 0, 1, 0.1);
        t0->set_server(s0);
        auto t1 = std::make_shared<task>(sim, 1, 1, 0.1);
        t1->set_server(s1);

        auto p_idle = plat->processors.at(0);
        p_idle->change_state(processor::state::idle);

        auto p_run = plat->processors.at(1);
        p_run->change_state(processor::state::running);
        p_run->set_server(s0);

        auto p_sleep = plat->processors.at(2);
        p_sleep->change_state(processor::state::sleep);

        EXPECT_FALSE(sched_parallel::processor_order(*p_idle, *p_idle));
        EXPECT_FALSE(sched_parallel::processor_order(*p_idle, *p_run));
        EXPECT_TRUE(sched_parallel::processor_order(*p_idle, *p_sleep));
        EXPECT_TRUE(sched_parallel::processor_order(*p_run, *p_idle));
        EXPECT_TRUE(sched_parallel::processor_order(*p_run, *p_run));
        EXPECT_TRUE(sched_parallel::processor_order(*p_run, *p_sleep));
        EXPECT_FALSE(sched_parallel::processor_order(*p_sleep, *p_idle));
        EXPECT_FALSE(sched_parallel::processor_order(*p_sleep, *p_run));
        EXPECT_FALSE(sched_parallel::processor_order(*p_sleep, *p_sleep));
}

int main(int argc, char** argv)
{
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
}
