#include "platform.hpp"
#include "engine.hpp"
#include "processor.hpp"
#include "protocols/traces.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <vector>

#include <iostream>

platform::platform(
    const std::weak_ptr<engine>& sim,
    size_t nb_proc,
    const std::vector<double>& frequencies,
    const double& effective_freq,
    bool freescaling_allowed)
    : entity(sim), frequencies(std::move(frequencies)), effective_freq(effective_freq),
      current_freq(0), freescaling(freescaling_allowed)
{
        assert(nb_proc > 0);
        assert(std::all_of(
            frequencies.begin(), frequencies.end(), [](double freq) { return freq > 0; }));
        assert(std::any_of(frequencies.begin(), frequencies.end(), [effective_freq](double freq) {
                return freq == effective_freq;
        }));

        std::sort(this->frequencies.begin(), this->frequencies.end(), std::greater<>());

        set_freq(freq_max());

        for (size_t i = 1; i <= nb_proc; ++i) {
                auto new_proc = std::make_shared<processor>(sim, i);
                processors.push_back(std::move(new_proc));
        }

        dvfs_timer = std::make_shared<timer>(sim, [this]() { set_freq(dvfs_target); });
}

void platform::set_freq(const double& new_freq)
{
        if (new_freq < 0 || new_freq > freq_max()) {
                throw std::domain_error("This frequency is not available");
        }

        double target_freq = freescaling ? new_freq : ceil_to_mode(new_freq);

        if (current_freq != target_freq) {
                current_freq = target_freq;
                sim()->add_trace(protocols::traces::frequency_update{current_freq});
        }
}

auto platform::ceil_to_mode(const double& freq) -> double
{
        assert(!frequencies.empty());
        auto last = *(frequencies.begin());
        for (auto it = frequencies.begin(); it != frequencies.end(); ++it) {
                auto opp = *it;
                if (opp < freq) { return last; }
                last = opp;
        }
        return freq_min();
}

void platform::dvfs_change_freq(const double& next_freq)
{
        using enum processor::state;

        if (next_freq < 0 || next_freq > freq_max()) {
                throw std::domain_error("This frequency is not available");
        }

        double target_freq = freescaling ? next_freq : ceil_to_mode(next_freq);
        if (target_freq == current_freq) { return; }

        if (!dvfs_timer->is_active()) {
                dvfs_target = target_freq;
                for (auto proc : processors) {
                        proc->dvfs_change_state(DVFS_DELAY);
                }
                dvfs_timer->set(DVFS_DELAY);
        }
        else {
                for (const auto& proc : processors) {
                        assert(proc->get_state() == processor::state::change);
                }
        }
}
