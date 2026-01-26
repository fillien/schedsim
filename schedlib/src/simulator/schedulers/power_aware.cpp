#include <cassert>
#include <simulator/schedulers/power_aware.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace scheds {

auto PowerAware::nb_active_procs() const -> std::size_t { return chip()->processors().size(); }

void PowerAware::update_platform()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto NB_PROCS{static_cast<double>(chip()->processors().size())};
        const auto TOTAL_U{total_utilization()};
        const auto F_MAX{chip()->freq_max()};
        const auto new_freq{(F_MAX * ((NB_PROCS - 1) * u_max() + TOTAL_U)) / NB_PROCS};

        assert(new_freq <= chip()->freq_max());
        if (chip()->freq() != chip()->ceil_to_mode(new_freq)) {
                for (const auto& proc : chip()->processors()) {
                        remove_task_from_cpu(proc.get());
                }
                chip()->dvfs_change_freq(new_freq);
                request_resched();
        }
}

} // namespace scheds
