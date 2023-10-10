#include <cassert>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
#include "plateform.hpp"
#include "sched_mono.hpp"
#include "sched_parallel.hpp"
#include "scheduler.hpp"
#include "server.hpp"
#include "task.hpp"
#include "tracer.hpp"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/yaml.h"

int main(const int argc, const char** argv) {
        constexpr double START_TIME = 0;

        using namespace std;

        // Check that there is a scenario file pass by argument
        if (argc < 2) {
                std::cerr << "No input scenario\n";
                return EXIT_FAILURE;
        }

        // Open the scenario
        YAML::Node config = YAML::LoadFile(argv[1]);

        // Create the simulation engine and attache to it a scheduler
        auto sim = make_shared<engine>();

        // Insert the plateform configured through the scenario file, in the simulation engine
        auto config_plat = make_shared<plateform>(sim, config["cores"].as<int>());
        sim->set_plateform(config_plat);

        assert(!config_plat->processors.empty());

        std::shared_ptr<scheduler> sched;
        if (config_plat->processors.size() == 1) {
                sched = std::dynamic_pointer_cast<scheduler>(
                    make_shared<sched_mono>(sim, config_plat->processors.at(0)));
        } else {
                sched = std::dynamic_pointer_cast<scheduler>(make_shared<sched_parallel>(sim));
        }
        sim->set_scheduler(sched);

        std::cout << "Platform: " << config_plat->processors.size() << " processors\n";

        // Prepare a vector to get all the tasks from the parsed scenario file
        std::vector<std::shared_ptr<task>> tasks{config["tasks"].size()};

        // Create tasks and job arrival events
        for (auto node : config["tasks"]) {
                auto new_task =
                    make_shared<task>(sim, node["id"].as<int>(), node["period"].as<double>(),
                                      node["utilization"].as<double>());

                // For each job of tasks add a "job arrival" event in the future list
                for (auto job : node["jobs"]) {
                        sim->add_event({types::JOB_ARRIVAL, new_task, job["duration"].as<double>()},
                                       job["arrival"].as<double>());
                }
                tasks.push_back(std::move(new_task));
        }

        // Simulate the system (job set + platform) with the chosen scheduler
        sim->simulation();

        // Print logs from the simulation
        // cout << "Logs :\n" << sim->logging_system.format(to_txt);
        std::cout << "Simulation ended" << std::endl;

        return EXIT_SUCCESS;
}
