#include "timer.hpp"
#include "engine.hpp"
#include "event.hpp"
#include <variant>
#include <vector>

void timer::set(const double& duration)
{
        sim()->add_event(events::timer_isr{shared_from_this()}, sim()->time() + duration);
}

void timer::cancel()
{
        std::erase_if(sim()->future_list, [&](const auto& entry) {
                if (const auto& evt = std::get_if<events::timer_isr>(&entry.second)) {
                        return evt->target_timer.get() == this;
                }
                return false;
        });
}
