#include "engine.hpp"
#include "event.hpp"
#include "plateform.hpp"
#include "scheduler.hpp"
#include "tracer_json.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <ios>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <fstream>

void engine::add_event(const events::event& new_event, const double& timestamp)
{
        future_list.insert({timestamp, std::move(new_event)});
}

void engine::add_trace(const events::event& new_trace)
{
        past_list.insert({current_timestamp, std::move(new_trace)});
}

void engine::simulation()
{
	constexpr size_t MAX_LOGS_SIZE{500};
	std::ofstream logs;

        logs.open("out.json");
	logs << "[" << std::endl;
        logs.close();

	prepare_log_file();
	
        // Loop until all events have been executed
        while (!future_list.empty()) {
                // A vector to store all the event of the current timestamp
                std::vector<events::event> current_events;

                // Update current timestamp
                current_timestamp = future_list.begin()->first;

                //std::cout << "========= Time " << current_timestamp << " =========\n";

                // Loop until move all the event of the current timestamp
                while (!future_list.empty() && future_list.begin()->first <= current_timestamp) {
                        // Take the next event, store it, and remove the event of the future list
                        auto itr = future_list.begin();
                        current_events.push_back(std::move(itr->second));
                        future_list.erase(itr);
                }
                sched->handle(current_events);

		// Write logs when there become 
                if (this->past_list.size() >= MAX_LOGS_SIZE) {
			write_log_file(past_list);
                }
        }

        if (future_list.empty()) { add_trace(events::sim_finished{}); }

	write_log_file(past_list);
	finish_log_file();
}
