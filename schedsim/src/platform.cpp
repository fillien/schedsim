#include <algorithm>
#include <cassert>
#include <cstddef>
#include <engine.hpp>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <platform.hpp>
#include <processor.hpp>
#include <protocols/traces.hpp>
#include <vector>

cluster::cluster(
    const std::weak_ptr<engine>& sim,
    const std::size_t cid,
    const std::vector<double>& frequencies,
    const double& effective_freq,
    const double& perf_score)
    : entity(sim), id(cid), frequencies(std::move(frequencies)), effective_freq(effective_freq),
      current_freq(0), perf_score(perf_score)
{
        assert(std::all_of(
            frequencies.begin(), frequencies.end(), [](double freq) { return freq > 0; }));
        assert(std::any_of(frequencies.begin(), frequencies.end(), [effective_freq](double freq) {
                return freq == effective_freq;
        }));

        std::sort(this->frequencies.begin(), this->frequencies.end(), std::greater<>());

        set_freq(freq_max());

        dvfs_timer = std::make_shared<timer>(sim, [this]() { set_freq(dvfs_target); });
}

void cluster::create_procs(const std::size_t nb_procs)
{
        assert(nb_procs > 0);
        processors.reserve(nb_procs);

        for (std::size_t i = 0; i < nb_procs; ++i) {
                auto id = sim()->chip()->reserve_next_id();
                processors.push_back(std::make_shared<processor>(sim(), shared_from_this(), id));
        }
}

void cluster::set_freq(const double& new_freq)
{
        if (new_freq < 0 || new_freq > freq_max()) {
                throw std::domain_error("This frequency is not available");
        }

        double target_freq = sim()->chip()->isfreescaling() ? new_freq : ceil_to_mode(new_freq);

        if (current_freq != target_freq) {
                current_freq = target_freq;
                sim()->add_trace(protocols::traces::frequency_update{id, current_freq});
        }
}

auto cluster::ceil_to_mode(const double& freq) -> double
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

void cluster::dvfs_change_freq(const double& next_freq)
{
        using enum processor::state;

        if (next_freq < 0 || next_freq > freq_max()) {
                throw std::domain_error("This frequency is not available");
        }

        double target_freq = sim()->chip()->isfreescaling() ? next_freq : ceil_to_mode(next_freq);
        if (target_freq == current_freq) { return; }

        if (!sim()->is_delay_active()) {
                set_freq(next_freq);
                return;
        }

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

platform::platform(const std::weak_ptr<engine>& sim, bool freescaling_allowed)
    : entity(sim), freescaling(freescaling_allowed)
{
}
