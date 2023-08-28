#include "engine.hpp"
#include "event.hpp"
#include "plateform.hpp"
#include "scheduler.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>

auto print = [](auto const& map) {
        std::cout << "Begin ===================\n";
        for (const auto& pair : map) {
                std::cout << '{' << pair.first << ": " << pair.second << "}\n";
        }
        std::cout << "End =====================\n";
};

void engine::add_event(const event& new_event, const double timestamp) {
        // Define if the event need to be unique at the current timestamp
        switch (new_event.type) {
        // List of event who need to be unique
        case types::RESCHED: {
                auto it = future_list.find(timestamp);

                // If the event doesn't already exist, insert it
                if (it == future_list.end()) {
                        future_list.insert({timestamp, std::move(new_event)});
                }
        } break;

        // For other event, just insert it
        default: {
                future_list.insert({timestamp, std::move(new_event)});
        }
        }
}

void engine::simulation() {
        int cpt_burst{0};
        double deltatime{0};

        // Loop until all events have been executed
        while (!future_list.empty() && cpt_burst < 10) {
                // A vector to store all the event of the current timestamp
                std::vector<event> current_events;

                // Compute deltatime since the last time jump
                deltatime = future_list.begin()->first - current_timestamp;

                if (deltatime < 0) {
                        std::cout << "false timestamp = " << future_list.begin()->first << "\n";
                }

                // Update current timestamp
                current_timestamp = future_list.begin()->first;

                std::cout << "========= Time " << current_timestamp << " =========\n";
                std::cout << "deltatime = " << deltatime << '\n';

                // Loop until move all the event of the current timestamp
                while (!future_list.empty() && future_list.begin()->first <= current_timestamp) {
                        // Take the next event, store it, and remove the event of the future list
                        auto itr = future_list.begin();
                        current_events.push_back(std::move(itr->second));
                        future_list.erase(itr);
                }
                sched->handle(current_events, deltatime);
                cpt_burst++;
        }

        if (future_list.empty()) {
                logging_system.add_trace({current_timestamp, types::SIM_FINISHED, 0, 0});
        }
}
