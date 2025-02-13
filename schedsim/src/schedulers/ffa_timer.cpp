#include <algorithm>
#include <cassert>
#include <cmath>
#include <schedulers/ffa_timer.hpp>

namespace scheds {
ffa_timer::ffa_timer(const std::weak_ptr<engine>& sim) : parallel(sim)
{
        if (!sim.lock()->is_delay_active()) {
                throw std::runtime_error(
                    "Simulation without DVFS & DPM delays is not support for this scheduler");
        }
        using enum Processor::state;

        const auto processors = chip()->processors;
        nb_active_procs = processors.size();

        freq_after_cooldown = chip()->freq_max();
        timer_dvfs_cooldown = std::make_shared<timer>(sim, [this, sim]() {
                if (chip()->freq() != chip()->ceil_to_mode(freq_after_cooldown)) {
                        for (const auto& proc : chip()->processors) {
                                remove_task_from_cpu(proc);
                        }
                        chip()->dvfs_change_freq(freq_after_cooldown);
                }
        });
}

auto ffa_timer::compute_freq_min(
    const double& freq_max,
    const double& total_util,
    const double& max_util,
    const double& nb_procs) -> double
{
        return (freq_max * (total_util + (nb_procs - 1) * max_util)) / nb_procs;
}

auto ffa_timer::get_nb_active_procs([[maybe_unused]] const double& new_utilization = 0) const
    -> std::size_t
{
        auto is_active = [](const auto& proc) {
                auto state = proc->get_state();
                return state == Processor::state::idle || state == Processor::state::running;
        };

        const auto& processors = chip()->processors;
        return std::count_if(processors.begin(), processors.end(), is_active);
}

void ffa_timer::change_state_proc(
    const Processor::state& next_state, const std::shared_ptr<Processor>& proc)
{
        assert(next_state != proc->get_state());
        assert(proc->get_state() != Processor::state::change);
        remove_task_from_cpu(proc);
        proc->dpm_change_state(next_state);
}

auto ffa_timer::cores_on_sleep() -> std::size_t
{
        using enum Processor::state;

        return std::count_if(
            chip()->processors.begin(), chip()->processors.end(), [](const auto& proc) {
                    return proc->get_state() == sleep;
            });
}

void ffa_timer::activate_next_core()
{
        using enum Processor::state;
        auto& processors = chip()->processors;
        auto itr = std::find_if(processors.begin(), processors.end(), [](const auto& proc) {
                return proc->get_state() == sleep;
        });
        if (itr == processors.end()) {
                return;
        } // No sleeping core found, there
          // is core in change state.
        change_state_proc(idle, *itr);
}

void ffa_timer::put_next_core_to_bed()
{
        using enum Processor::state;
        auto& processors = chip()->processors;
        auto itr = std::find_if(processors.begin(), processors.end(), [](const auto& proc) {
                return proc->get_state() == idle || proc->get_state() == running;
        });
        assert(itr != processors.end());
        change_state_proc(sleep, *itr);
}

void ffa_timer::adjust_active_processors(std::size_t target_processors)
{
        if (target_processors > get_nb_active_procs()) {
                for (std::size_t i = 0; i < target_processors - get_nb_active_procs(); ++i) {
                        activate_next_core();
                }
        }
        else if (target_processors < get_nb_active_procs()) {
                for (std::size_t i = 0; i < get_nb_active_procs() - target_processors; ++i) {
                        put_next_core_to_bed();
                }
        }
}

void ffa_timer::update_platform()
{
        const double total_util{get_active_bandwidth()};
        const double max_util{u_max()};
        const double max_procs{static_cast<double>(chip()->processors.size())};
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
        int diff_dpm = static_cast<std::size_t>(next_active_procs) - get_nb_active_procs();
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
                            [](const std::shared_ptr<timer>& a, const std::shared_ptr<timer>& b) {
                                    return a->get_deadline() < b->get_deadline();
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
                                auto timer_dpm = std::make_shared<timer>(
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
