#include <simulator/engine.hpp>
#include <simulator/event.hpp>
#include <simulator/timer.hpp>

#include <variant>

void Timer::fire()
{
        assert(active_);
        active_ = false;
        callback_();
}

void Timer::set(const double& duration)
{
        assert(active_ != true);
        active_ = true;
        deadline_ = sim().time() + duration;
        sim().add_event(events::TimerIsr{this}, deadline_);
}

void Timer::cancel()
{
        assert(active_ != false);
        active_ = false;
        auto cpt = sim().remove_event([&](const auto& entry) {
                if (const auto& evt = std::get_if<events::TimerIsr>(&entry.second)) {
                        return evt->target_timer == this;
                }
                return false;
        });
        assert(cpt > 0);
}
