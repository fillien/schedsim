#include <cassert>
#include <schedulers/power_aware_timer.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace scheds {

power_aware_timer::power_aware_timer(const std::weak_ptr<engine>& sim) : parallel(sim)
{
        if (!sim.lock()->is_delay_active()) {
                throw std::runtime_error(
                    "Simulation without DVFS & DPM delays is not support for this scheduler");
        }
};

auto power_aware_timer::get_nb_active_procs([[maybe_unused]] const double& new_utilization) const
    -> std::size_t
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        return chip()->processors.size();
}

void power_aware_timer::update_platform()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const double U_MAX{get_max_utilization(servers)};
        const double NB_PROCS{static_cast<double>(chip()->processors.size())};
        const double TOTAL_U{get_total_utilization()};
        const double F_MAX{chip()->freq_max()};
        const double new_freq{(F_MAX * ((NB_PROCS - 1) * U_MAX + TOTAL_U)) / NB_PROCS};

        assert(new_freq <= chip()->freq_max());
        if (chip()->freq() != chip()->ceil_to_mode(new_freq)) {
                for (const auto& proc : chip()->processors) {
                        remove_task_from_cpu(proc);
                }
                chip()->dvfs_change_freq(new_freq);
                call_resched();
        }
}

} // namespace scheds
