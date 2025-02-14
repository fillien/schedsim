#include <engine.hpp>
#include <event.hpp>
#include <map>
#include <timer.hpp>
#include <variant>

void Timer::fire()
{
        assert(active);
        active = false;
        callback();
}

void Timer::set(const double& duration)
{
        assert(active != true);
        active = true;
        deadline = sim()->time() + duration;
        sim()->add_event(events::TimerIsr{shared_from_this()}, deadline);
}

void Timer::cancel()
{
        assert(active != false);
        active = false;
        auto cpt = sim()->remove_event([&](const auto& entry) {
                if (const auto& evt = std::get_if<events::TimerIsr>(&entry.second)) {
                        return evt->target_timer.get() == this;
                }
                return false;
        });
        assert(cpt > 0);
}
