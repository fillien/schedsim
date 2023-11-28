#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
#include "plateform.hpp"
//#include "sched_mono.hpp"
#include "scenario.hpp"
#include "sched_parallel.hpp"
#include "scheduler.hpp"
#include "server.hpp"
#include "task.hpp"
#include "traces.hpp"

auto main(const int argc, const char** argv) -> int
{
        using namespace std;

        // Check that there is a scenario file pass by argument
        if (argc < 2) {
                std::cerr << "No input scenario\n";
                return EXIT_FAILURE;
        }

        // Open the scenario
        std::filesystem::path input_taskset{argv[1]};
        scenario::setting taskset = scenario::read_file(input_taskset);

        std::cout << "Coeurs: " << taskset.nb_cores
                  << "\nNombre de tâches: " << taskset.tasks.size() << "\n";

        // Create the simulation engine and attache to it a scheduler
        auto sim = make_shared<engine>();

        // Insert the plateform configured through the scenario file, in the simulation engine
        auto config_plat = make_shared<plateform>(sim, taskset.nb_cores);
        sim->set_plateform(config_plat);

        std::shared_ptr<scheduler> sched = make_shared<sched_parallel>(sim);
        sim->set_scheduler(sched);

        std::vector<std::shared_ptr<task>> tasks{taskset.tasks.size()};

        // Create tasks and job arrival events
        for (auto input_task : taskset.tasks) {
                auto new_task = make_shared<task>(
                    sim, input_task.id, input_task.period, input_task.utilization);

                // For each job of tasks add a "job arrival" event in the future list
                for (auto job : input_task.jobs) {
                        sim->add_event(events::job_arrival{new_task, job.duration}, job.arrival);
                }
                tasks.push_back(std::move(new_task));
        }

        // Simulate the system (job set + platform) with the chosen scheduler
        sim->simulation();
        std::cout << "Simulation ended" << std::endl;

        std::filesystem::path output_logs{"out.json"};
        traces::write_log_file(sim->get_traces(), output_logs);

        return EXIT_SUCCESS;
}
