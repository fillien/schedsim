#include <algorithm>
#include <cassert>
#include <schedulers/ffa_timer.hpp>

namespace scheds {
FfaTimer::FfaTimer(const std::weak_ptr<Engine>& sim) : DpmDvfs(sim)
{
        if (!sim.lock()->is_delay_activated()) {
                throw std::runtime_error(
                    "Simulation without DVFS & DPM delays is not support for this scheduler");
        }

        nb_active_procs_ = chip()->processors().size();

        freq_after_cooldown = chip()->freq_max();
        timer_dvfs_cooldown = std::make_shared<Timer>(sim, [this, sim]() {
                if (chip()->freq() != chip()->ceil_to_mode(freq_after_cooldown)) {
                        for (const auto& proc : chip()->processors()) {
                                remove_task_from_cpu(proc);
                        }
                        chip()->dvfs_change_freq(freq_after_cooldown);
                }
        });
}

void FfaTimer::update_platform()
{
        const double total_util{active_bandwidth()};
        const double max_util{u_max()};
        const double max_procs{static_cast<double>(chip()->processors().size())};
        const double freq_eff{chip()->freq_eff()};
        const double freq_max{chip()->freq_max()};
        const double freq_min{compute_freq_min(freq_max, total_util, max_util, max_procs)};

        double next_freq{0};
        double next_active_procs{0};

        if (freq_min < freq_eff) {
                next_freq = freq_eff;
                next_active_procs = std::ceil(max_procs * (freq_min / freq_eff));
        }
        else {
                assert(freq_min <= chip()->freq_max());
                next_freq = chip()->ceil_to_mode(freq_min);
                next_active_procs = max_procs;
        }

        next_active_procs = static_cast<std::size_t>(clamp(next_active_procs));
        assert(next_active_procs >= 1 && next_active_procs <= max_procs);

        // Manage DPM timers
        int diff_dpm = static_cast<std::size_t>(next_active_procs) - nb_active_procs();
        if (diff_dpm > 0) {
                for (int i = 0; i < diff_dpm; ++i) {
                        activate_next_core();
                }
        }
        else if (diff_dpm < 0) {
                // Compute the cover
                int couverture = diff_dpm + timers_dpm_cooldown.size();

                assert(couverture != 0);
                if (couverture > 0) {
                        // Cancel youngest timers
                        std::sort(
                            timers_dpm_cooldown.begin(),
                            timers_dpm_cooldown.end(),
                            [](const std::shared_ptr<Timer>& a, const std::shared_ptr<Timer>& b) {
                                    return a->deadline() < b->deadline();
                            });

                        for (int i = 0; i < diff_dpm; ++i) {
                                assert(!timers_dpm_cooldown.empty());
                                auto tmp = timers_dpm_cooldown.begin();
                                tmp->get()->cancel();
                                timers_dpm_cooldown.erase(tmp);
                        }
                }
                else if (couverture < 0) {
                        for (int i = couverture; i > 0; --i) {
                                auto timer_dpm = std::make_shared<Timer>(
                                    sim(), [this]() { activate_next_core(); });
                                timer_dpm->set(DPM_COOLDOWN);
                                timers_dpm_cooldown.push_back(std::move(timer_dpm));
                        }
                }
        }

        // Manage DVFS timer
        if (chip()->freq() != chip()->ceil_to_mode(next_freq)) {
                freq_after_cooldown = chip()->ceil_to_mode(next_freq);
                if (!timer_dvfs_cooldown->is_active()) { timer_dvfs_cooldown->set(DVFS_COOLDOWN); }
        }
        else if (timer_dvfs_cooldown->is_active()) {
                timer_dvfs_cooldown->cancel();
        }
}

} // namespace scheds
