#include <algorithm>
#include <cassert>
#include <cmath>
#include <simulator/schedulers/ffa.hpp>

namespace scheds {

void Ffa::update_platform()
{
        const double total_util{active_bandwidth()};
        const double max_util{u_max()};
        const double max_procs{static_cast<double>(chip()->processors().size())};
        const double freq_eff{chip()->freq_eff()};
        const double freq_max{chip()->freq_max()};
        const double freq_min{std::min(compute_freq_min(freq_max, total_util, max_util, max_procs), freq_max)};

        double next_freq{0};
        double next_active_procs{0};

        if (freq_min < freq_eff) {
                next_freq = freq_eff;
                next_active_procs = std::ceil(max_procs * (freq_min / freq_eff));
        }
        else {
                next_freq = chip()->ceil_to_mode(freq_min);
                next_active_procs = max_procs;
        }

        next_active_procs = clamp(next_active_procs);
        assert(next_active_procs >= 1 && next_active_procs <= max_procs);

        adjust_active_processors(static_cast<std::size_t>(next_active_procs));
        if (chip()->freq() != chip()->ceil_to_mode(next_freq)) {
                for (const auto& proc : chip()->processors()) {
                        remove_task_from_cpu(proc.get());
                }
                chip()->dvfs_change_freq(next_freq);
                request_resched();
        }
}

} // namespace scheds
