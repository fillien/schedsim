#include "processor.hpp"
#include "timer.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <memory>
#include <schedulers/ffa_timer.hpp>
#include <vector>

ffa_timer::ffa_timer(const std::weak_ptr<engine> sim) : sched_parallel(sim)
{
        using enum processor::state;

        const auto processors = sim.lock()->chip()->processors;
        nb_active_procs = processors.size();
}

auto ffa_timer::compute_freq_min(
    const double& freq_max,
    const double& total_util,
    const double& max_util,
    const double& nb_procs) -> double
{
        return (freq_max * (total_util + (nb_procs - 1) * max_util)) / nb_procs;
}

auto ffa_timer::get_nb_active_procs(const double& new_utilization = 0) const -> std::size_t
{
        return std::count_if(
            sim()->chip()->processors.begin(), sim()->chip()->processors.end(), [](auto prc) {
                    return prc->get_state() == processor::state::idle ||
                           prc->get_state() == processor::state::running;
            });
}

void ffa_timer::change_state_proc(
    const processor::state next_state, const std::shared_ptr<processor>& proc)
{
        using enum processor::state;

        assert(next_state != proc->get_state());

        // Is the proc already in CHANGE mode ?
        // Is the proc there a timer that already active that is on this proc

        remove_task_from_cpu(proc);
        proc->change_state(change);
        proc->coretimer->callback = [this, proc, next_state]() {
                proc->change_state(next_state);
                this->need_resched = true;
        };
        proc->coretimer->set(DELAY_CORE_CHANGE);
}

auto ffa_timer::cores_on_timer() -> std::size_t
{
        return std::count_if(core_timers.begin(), core_timers.end(), [](const auto& timer) {
                return timer->is_active();
        });
}

auto ffa_timer::cores_on_sleep() -> std::size_t
{
        using enum processor::state;

        return std::count_if(
            sim()->chip()->processors.begin(),
            sim()->chip()->processors.end(),
            [](const auto& proc) { return proc->get_state() == sleep; });
}

void ffa_timer::activate_next_core()
{
        using enum processor::state;
        auto& processors = sim()->chip()->processors;
        auto it = std::find_if(processors.begin(), processors.end(), [](const auto& proc) {
                return proc->get_state() == sleep;
        });
        if (it == processors.end()) {
                return;
        } // No sleeping core found, there
          // is core in change state.
        change_state_proc(idle, *it);
}

void ffa_timer::put_next_core_to_bed()
{
        using enum processor::state;
        auto& processors = sim()->chip()->processors;
        auto it = std::find_if(processors.begin(), processors.end(), [](const auto& proc) {
                return proc->get_state() == idle || proc->get_state() == running;
        });
        assert(it != processors.end());
        change_state_proc(sleep, *it);
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

void ffa_timer::adjust_frequency(const std::size_t target_freq)
{
        using enum processor::state;

        assert(target_freq != sim()->chip()->freq());
        for (auto procs : sim()->chip()->processors) {
                remove_task_from_cpu(procs);
                procs->change_state(change);
        }
        auto isr = std::make_shared<timer>(sim(), [target_freq, this]() {
                sim()->chip()->set_freq(target_freq);
                for (auto& procs : sim()->chip()->processors) {
                        procs->change_state(idle);
                }
                this->need_resched = true;
        });
        isr->set(DELAY_FREQUENCY);
}

void ffa_timer::update_platform()
{
        const auto chip = sim()->chip();
        const double total_util{get_active_bandwidth()};
        const double max_util{get_max_utilization(servers)};
        const double max_procs{static_cast<double>(chip->processors.size())};
        const double freq_eff{sim()->chip()->freq_eff()};
        const double freq_max{sim()->chip()->freq_max()};
        const double freq_min{compute_freq_min(freq_max, total_util, max_util, max_procs)};

        double next_freq{0};
        double next_active_procs{0};

        if (freq_min < freq_eff) {
                next_freq = freq_eff;
                next_active_procs = std::ceil(max_procs * (freq_min / freq_eff));
        }
        else {
                assert(freq_min <= chip->freq_max());
                next_freq = chip->ceil_to_mode(freq_min);
                next_active_procs = max_procs;
        }

        next_active_procs = static_cast<std::size_t>(clamp(next_active_procs));
        assert(next_active_procs >= 1 && next_active_procs <= max_procs);

        adjust_active_processors(static_cast<std::size_t>(next_active_procs));
        // this->nb_active_procs = static_cast<std::size_t>(next_active_procs);
        chip->set_freq(next_freq);
        // if (chip->freq() != next_freq) {
        //         adjust_frequency(next_freq);
        //         // chip->set_freq(next_freq);
        // }
}
