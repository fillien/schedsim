#include "platform.hpp"
#include "engine.hpp"
#include "processor.hpp"
#include "protocols/traces.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <set>
#include <vector>

platform::platform(
    const std::weak_ptr<engine>& sim,
    size_t nb_proc,
    const std::vector<double>& frequencies,
    bool freescaling_allowed)
    : entity(sim), frequencies(std::move(frequencies)), freescaling(freescaling_allowed)
{
        assert(nb_proc > 0);
        assert(std::all_of(frequencies.begin(), frequencies.end(), [](double f) { return f > 0; }));

        std::sort(this->frequencies.begin(), this->frequencies.end(), std::less<>());

        current_freq = *(frequencies.begin());
        set_freq(*(frequencies.begin()));

        for (size_t i = 1; i <= nb_proc; ++i) {
                auto new_proc = std::make_shared<processor>(sim, i);
                processors.push_back(std::move(new_proc));
        }
}

void platform::set_freq(const double& new_freq)
{
        if (freescaling && new_freq <= *frequencies.begin() && new_freq >= 0) {
                current_freq = new_freq;
                sim()->add_trace(protocols::traces::frequency_update{current_freq});
        }
        else if (std::find(frequencies.begin(), frequencies.end(), new_freq) != frequencies.end()) {
                current_freq = new_freq;
                sim()->add_trace(protocols::traces::frequency_update{current_freq});
        }
        else {
                throw std::domain_error("This frequency is not available");
        }
}
