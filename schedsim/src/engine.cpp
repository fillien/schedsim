#include <allocator.hpp>
#include <engine.hpp>
#include <event.hpp>
#include <map>
#include <protocols/traces.hpp>
#include <timer.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

Engine::Engine(const bool is_there_delay) : delay_activated_(is_there_delay) {}

void Engine::add_event(const events::Event& new_event, const double& timestamp)
{
        future_list_.insert({timestamp, std::move(new_event)});
}

void Engine::add_trace(const protocols::traces::trace& new_trace)
{
        past_list_.insert({current_timestamp_, std::move(new_trace)});
}

void Engine::simulation()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif

        // Loop until all events have been executed
        while (!future_list_.empty()) {
                // A vector to store all the event of the current timestamp
                std::vector<events::Event> current_events;

                // Update current timestamp
                current_timestamp_ = future_list_.begin()->first;

                // Loop until move all the event of the current timestamp
                while (!future_list_.empty() && future_list_.begin()->first <= current_timestamp_) {
                        // Take the next event, store it, and remove the event of the future list
                        auto itr = future_list_.begin();
                        current_events.push_back(std::move(itr->second));
                        future_list_.erase(itr);
                }

                // Pass the current events to the scheduler for handling
                sched_->handle(current_events);
        }

        // Add a simulation finished trace to the past list
        if (future_list_.empty()) { add_trace(protocols::traces::SimFinished{}); }
}
