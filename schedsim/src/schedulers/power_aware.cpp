#include <cassert>
#include <schedulers/power_aware.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

auto sched_power_aware::get_nb_active_procs([[maybe_unused]] const double& new_utilization) const
    -> std::size_t
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        return sim()->chip()->processors.size();
}

void sched_power_aware::update_platform()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto chip = sim()->chip();
        const auto U_MAX{get_max_utilization(servers)};
        const auto NB_PROCS{static_cast<double>(chip->processors.size())};
        const auto TOTAL_U{get_total_utilization()};
        const auto F_MAX{chip->freq_max()};
        const auto new_freq{(F_MAX * ((NB_PROCS - 1) * U_MAX + TOTAL_U)) / NB_PROCS};

        assert(new_freq <= chip->freq_max());
        if (chip->freq() != chip->ceil_to_mode(new_freq)) {
                for (const auto& proc : chip->processors) {
                        remove_task_from_cpu(proc);
                }
                chip->dvfs_change_freq(new_freq);
                this->call_resched();
        }
}
