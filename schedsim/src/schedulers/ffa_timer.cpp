#include "processor.hpp"
#include "timer.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <memory>
#include <ranges>
#include <schedulers/ffa_timer.hpp>
#include <vector>

#include <iostream>

auto ffa_timer::compute_freq_min(
    const double& freq_max,
    const double& total_util,
    const double& max_util,
    const double& nb_procs) -> double
{
        return (freq_max * (total_util + (nb_procs - 1) * max_util)) / nb_procs;
}

auto ffa_timer::get_nb_active_procs(const double& new_utilization) const -> std::size_t
{
        return nb_active_procs;
}

void ffa_timer::change_state_proc(
    const processor::state next_state, const std::shared_ptr<processor>& proc)
{
        if (next_state == processor::state::idle) {
                // The core must be active
                assert(proc->get_state() != processor::state::idle);
                proc->change_state(processor::state::change);
                auto isr = std::make_shared<timer>(
                    sim(), [&]() { proc->change_state(processor::state::idle); });
                isr->set(DELAY_CORE_CHANGE);
        }
        else if (next_state == processor::state::sleep) {
                // The core must be sleeping
                assert(proc->get_state() != processor::state::sleep);
                remove_task_from_cpu(proc);
                proc->change_state(processor::state::change);
                auto isr = std::make_shared<timer>(
                    sim(), [&]() { proc->change_state(processor::state::sleep); });
                isr->set(DELAY_CORE_CHANGE);
        }
        else {
                assert(false);
        }
}

auto ffa_timer::cores_on_timer() -> std::size_t
{
        return std::count_if(std::begin(core_timers), std::end(core_timers), [](const auto& tt) {
                return tt->is_active();
        });
}

auto ffa_timer::cores_on_sleep() -> std::size_t
{
        return std::count_if(
            std::begin(sim()->chip()->processors),
            std::end(sim()->chip()->processors),
            [](const auto& proc) { return proc->get_state() == processor::state::sleep; });
}

void ffa_timer::cancel_next_timer()
{
        auto tr = std::max_element(
            std::begin(core_timers),
            std::end(core_timers),
            [](const auto& first, const auto& second) {
                    if (first->is_active()) {
                            if (second->is_active()) {
                                    return first->get_deadline() < second->get_deadline();
                            }
                            return false;
                    }
                    return true;
            });
        if ((*tr)->is_active()) { (*tr)->cancel(); }
}

void ffa_timer::activate_next_core()
{
        using enum processor::state;

        std::vector<std::shared_ptr<processor>> copy_chip = sim()->chip()->processors;
        std::sort(copy_chip.begin(), copy_chip.end(), from_shared<processor>(processor_order));
        auto it = copy_chip.cbegin();

        bool found_sleep_proc{false};
        while (!found_sleep_proc) {
                if ((*it)->get_state() == sleep) {
                        change_state_proc(idle, (*it));
                        found_sleep_proc = true;
                }
        }
}

void ffa_timer::put_next_core_to_bed()
{
        using enum processor::state;

        std::vector<std::shared_ptr<processor>> copy_chip = sim()->chip()->processors;
        std::sort(copy_chip.begin(), copy_chip.end(), from_shared<processor>(processor_order));
        auto it = copy_chip.cbegin();

        bool found_idle_proc{false};
        while (!found_idle_proc) {
                if ((*it)->get_state() != sleep) {
                        core_timers.at((*it)->get_id())->set(DELAY_BEFORE_SLEEP);
                        found_idle_proc = true;
                }
        }
}

void ffa_timer::update_platform()
{
        double next_active_procs{0};

        const double total_util{get_active_bandwidth()};
        const double max_util{get_max_utilization(servers)};
        const double max_nb_procs{static_cast<double>(sim()->chip()->processors.size())};
        const auto freq_eff{sim()->chip()->freq_eff()};
        const auto freq_max{sim()->chip()->freq_max()};

        const auto freq_min{compute_freq_min(freq_max, total_util, max_util, max_nb_procs)};
        double next_freq{0};

        if (freq_min < freq_eff) {
                next_freq = freq_eff;
                next_active_procs = std::ceil(max_nb_procs * (freq_min / freq_eff));
        }
        else {
                assert(freq_min <= sim()->chip()->freq_max());
                next_freq = sim()->chip()->ceil_to_mode(freq_min);
                next_active_procs = max_nb_procs;
        }

        next_active_procs = static_cast<std::size_t>(clamp(next_active_procs));

        assert(nb_active_procs >= 1 && nb_active_procs <= sim()->chip()->processors.size());

        if (next_active_procs > this->nb_active_procs) {
                // On cherche des coeurs en plus
                auto diff = next_active_procs - this->nb_active_procs;
                std::cout << "looking for " << diff << " more cores" << std::endl;
                auto copy_active =
                    sim()->chip()->processors | std::ranges::views::filter([](auto core) {
                            return core->get_state() == processor::state::running ||
                                   core->get_state() == processor::state::idle;
                    });
                while (diff) {
                        assert(copy_active.begin() != copy_active.end());
                        change_state_proc(processor::state::sleep, *copy_active.begin());
                        std::cout << "added a core" << std::endl;
                        diff -= 1;
                }
        }
        else if (next_active_procs < this->nb_active_procs) {
                // On cherche des coeurs en moins
                auto diff = this->nb_active_procs - next_active_procs;
                std::cout << "looking for " << diff << " less cores" << std::endl;

                while (diff) {
                        auto copy_active =
                            sim()->chip()->processors | std::ranges::views::filter([](auto core) {
                                    return core->get_state() == processor::state::running ||
                                           core->get_state() == processor::state::idle;
                            });

                        assert(copy_active.begin() != copy_active.end());
                        change_state_proc(processor::state::sleep, *copy_active.begin());
                        std::cout << "remove a core" << std::endl;
                        diff -= 1;
                }
        }

        this->nb_active_procs = next_active_procs;
}
