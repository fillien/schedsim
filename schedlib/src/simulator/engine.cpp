#include <protocols/traces.hpp>
#include <simulator/allocator.hpp>
#include <simulator/engine.hpp>
#include <simulator/event.hpp>
#include <simulator/timer.hpp>

#include <map>

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

        for (auto clu : platform_->clusters()) {
                clu->scheduler().lock()->call_resched();
        }

        while (!future_list_.empty()) {
#ifdef TRACY_ENABLE
                FrameMark;
#endif
                std::vector<events::Event> current_events;

                current_timestamp_ = future_list_.begin()->first;

                auto [range_begin, range_end] = future_list_.equal_range(current_timestamp_);
                for (auto it = range_begin; it != range_end; ++it) {
                        current_events.push_back(std::move(it->second));
                }
                future_list_.erase(range_begin, range_end);

                alloc_->handle(current_events);
        }

        if (future_list_.empty()) { add_trace(protocols::traces::SimFinished{}); }
}
