#include <algorithm>
#include <schedulers/dpm_dvfs.hpp>

namespace scheds {
auto DpmDvfs::compute_freq_min(
    const double& freq_max,
    const double& total_util,
    const double& max_util,
    const double& nb_procs) -> double
{
        return (freq_max * (total_util + (nb_procs - 1) * max_util)) / nb_procs;
}

auto DpmDvfs::nb_active_procs() const -> std::size_t
{
        auto is_active = [](const auto& proc) {
                auto state = proc->state();
                return state == Processor::State::Idle || state == Processor::State::Running;
        };

        const auto& processors = chip()->processors();
        return std::count_if(processors.begin(), processors.end(), is_active);
}

void DpmDvfs::change_state_proc(
    const Processor::State& next_state, const std::shared_ptr<Processor>& proc)
{
        assert(next_state != proc->state());
        assert(proc->state() != Processor::State::Change);
        remove_task_from_cpu(proc);
        proc->dpm_change_state(next_state);
}

void DpmDvfs::activate_next_core()
{
        using enum Processor::State;
        const auto& processors = chip()->processors();
        auto itr = std::ranges::find_if(
            processors, [](const auto& proc) { return proc->state() == Sleep; });
        if (itr == processors.end()) {
                return;
        } // No sleeping core found, there
          // is core in change state.
        change_state_proc(Idle, *itr);
}

void DpmDvfs::put_next_core_to_bed()
{
        using enum Processor::State;
        const auto& processors = chip()->processors();
        auto itr = std::ranges::find_if(processors, [](const auto& proc) {
                return proc->state() == Idle || proc->state() == Running;
        });
        assert(itr != processors.end());
        change_state_proc(Sleep, *itr);
}

void DpmDvfs::adjust_active_processors(const std::size_t target_processors)
{
        if (target_processors > nb_active_procs()) {
                for (std::size_t i = 0; i < target_processors - nb_active_procs(); ++i) {
                        activate_next_core();
                }
        }
        else if (target_processors < nb_active_procs()) {
                for (std::size_t i = 0; i < nb_active_procs() - target_processors; ++i) {
                        put_next_core_to_bed();
                }
        }
}

} // namespace scheds
