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

        if (argc != 2) {
                std::cerr << "No input scenario\n";
                return EXIT_FAILURE;
        }

        // Open the scenario
        YAML::Node config = YAML::LoadFile(argv[1]);

        auto sim = make_shared<engine>(config["cores"].as<int>());
        auto sched = make_shared<scheduler>();
        sim->set_scheduler(sched);

        std::vector<std::shared_ptr<task>> tasks{config["tasks"].size()};
        // Create tasks and job arrival events
        for (auto node : config["tasks"]) {
                auto new_task = make_shared<task>(node["id"].as<int>(), node["period"].as<double>(),
                                                  node["utilization"].as<double>());

                for (auto job : node["jobs"]) {
                        sim->add_event({types::JOB_ARRIVAL, new_task, job["duration"].as<double>()},
                                       job["arrival"].as<double>());
                }
                tasks.push_back(std::move(new_task));
        }

        // std::weak_ptr<server> null;
        // sim->add_event({types::SIM_FINISHED, null, 0}, 11);

        // Simulate the system (job set + platform) with the chosen scheduler
        sim->simulation();

        cout << "Logs :\n" << sim->logging_system.format(to_txt);

        return EXIT_SUCCESS;
}
