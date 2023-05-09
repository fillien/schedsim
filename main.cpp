#include <iostream>
#include <memory>

#include "engine.hpp"
#include "event.hpp"
#include "scheduler.hpp"
#include "task.hpp"
#include "tracer.hpp"

int main() {
        using namespace std;

        constexpr size_t NB_PROCESSOR = 1;
        constexpr double START_TIME = 0;

        auto sim = make_shared<engine>(NB_PROCESSOR);
        auto sched = make_shared<scheduler>();
        sim->set_scheduler(sched);

        // Execute task 1 a time 0, for a duration of 5 units
        auto t1 = make_shared<task>(1, 4, 0.5);
        auto t2 = make_shared<task>(2, 6, 0.5);
        sim->add_event({types::JOB_ARRIVAL, t1, 5}, START_TIME);
        sim->add_event({types::JOB_ARRIVAL, t2, 8}, START_TIME);

        // Simulate the system (job set + platform) with the chosen scheduler
        sim->simulation();

        cout << "Logs :\n" << sim->logging_system.format(to_txt);

        return EXIT_SUCCESS;
}
