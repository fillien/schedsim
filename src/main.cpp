#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
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
        if (argc != 2) {
                std::cerr << "No input scenario\n";
                return EXIT_FAILURE;
        }

        // Open the scenario
        YAML::Node config = YAML::LoadFile(argv[1]);

        // Create the simulation engine and attache to it a scheduler
        auto sim = make_shared<engine>(config["cores"].as<int>());
        auto sched = make_shared<scheduler>();
        sim->set_scheduler(sched);

        // Prepare a vector to get all the tasks from the parsed scenario file
        std::vector<std::shared_ptr<task>> tasks{config["tasks"].size()};

        // Create tasks and job arrival events
        for (auto node : config["tasks"]) {
                auto new_task = make_shared<task>(node["id"].as<int>(), node["period"].as<double>(),
                                                  node["utilization"].as<double>());

                // For each job of tasks add a "job arrival" event in the future list
                for (auto job : node["jobs"]) {
                        sim->add_event({types::JOB_ARRIVAL, new_task, job["duration"].as<double>()},
                                       job["arrival"].as<double>());
                }
                tasks.push_back(std::move(new_task));
        }

        // Insert a event to force finishing the simulation at precise time
        std::weak_ptr<server> null;
        sim->add_event({types::SIM_FINISHED, null, 0}, 1.5);

        // Simulate the system (job set + platform) with the chosen scheduler
        sim->simulation();

        // Print logs from the simulation
        cout << "Logs :\n" << sim->logging_system.format(to_txt);

        return EXIT_SUCCESS;
}
