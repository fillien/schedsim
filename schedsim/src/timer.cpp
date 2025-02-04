#include <engine.hpp>
#include <event.hpp>
#include <map>
#include <timer.hpp>
#include <variant>

void timer::fire()
{
        assert(active);
        active = false;
        callback();
}

void timer::set(const double& duration)
{
        assert(active != true);
        active = true;
        deadline = sim()->time() + duration;
        sim()->add_event(events::timer_isr{shared_from_this()}, deadline);
}

void timer::cancel()
{
        assert(active != false);
        active = false;
        auto cpt = std::erase_if(sim()->future_list, [&](const auto& entry) {
                if (const auto& evt = std::get_if<events::timer_isr>(&entry.second)) {
                        return evt->target_timer.get() == this;
                }
                return false;
        });
        assert(cpt > 0);
}
