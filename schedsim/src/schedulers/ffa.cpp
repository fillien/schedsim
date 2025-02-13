#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <schedulers/ffa.hpp>

namespace scheds {
ffa::ffa(const std::weak_ptr<engine>& sim) : parallel(sim) {}

void ffa::set_cluster(const std::weak_ptr<Cluster>& clu)
{
        attached_cluster = clu;
        nb_active_procs = chip()->processors.size();
}

auto ffa::compute_freq_min(
    const double& freq_max,
    const double& total_util,
    const double& max_util,
    const double& nb_procs) -> double
{
        return (freq_max * (total_util + (nb_procs - 1) * max_util)) / nb_procs;
}

auto ffa::get_nb_active_procs([[maybe_unused]] const double& new_utilization = 0) const
    -> std::size_t
{
        auto is_active = [](const auto& proc) {
                auto state = proc->get_state();
                return state == Processor::state::idle || state == Processor::state::running;
        };

        const auto& processors = chip()->processors;
        return std::count_if(processors.begin(), processors.end(), is_active);
}

void ffa::change_state_proc(
    const Processor::state& next_state, const std::shared_ptr<Processor>& proc)
{
        assert(next_state != proc->get_state());
        assert(proc->get_state() != Processor::state::change);
        remove_task_from_cpu(proc);
        proc->dpm_change_state(next_state);
}

void ffa::activate_next_core()
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

void ffa::put_next_core_to_bed()
{
        using enum Processor::state;
        auto& processors = chip()->processors;
        auto itr = std::find_if(processors.begin(), processors.end(), [](const auto& proc) {
                return proc->get_state() == idle || proc->get_state() == running;
        });
        assert(itr != processors.end());
        change_state_proc(sleep, *itr);
}

void ffa::adjust_active_processors(const std::size_t target_processors)
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

void ffa::update_platform()
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

        next_active_procs = clamp(next_active_procs);
        assert(next_active_procs >= 1 && next_active_procs <= max_procs);

        adjust_active_processors(static_cast<std::size_t>(next_active_procs));
        if (chip()->freq() != chip()->ceil_to_mode(next_freq)) {
                for (const auto& proc : chip()->processors) {
                        remove_task_from_cpu(proc);
                }
                chip()->dvfs_change_freq(next_freq);
                sim()->get_sched()->call_resched(shared_from_this());
        }
}

} // namespace scheds
