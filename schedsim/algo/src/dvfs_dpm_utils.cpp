#include <schedsim/algo/dvfs_dpm_utils.hpp>

#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/processor.hpp>

#include <algorithm>
#include <cmath>

namespace schedsim::algo::dvfs_dpm {

double compute_freq_min(double freq_max, double total_util, double max_util, double nb_procs) {
    if (nb_procs <= 0.0) {
        return freq_max;
    }
    return freq_max * (total_util + (nb_procs - 1.0) * max_util) / nb_procs;
}

std::size_t clamp_procs(double value, std::size_t max_procs) {
    if (value < 1.0) {
        return 1;
    }
    auto rounded = static_cast<std::size_t>(std::ceil(value));
    return std::min(rounded, max_procs);
}

std::size_t count_active_processors(std::span<core::Processor* const> procs) {
    std::size_t count = 0;
    for (auto* proc : procs) {
        auto state = proc->state();
        if (state == core::ProcessorState::Idle || state == core::ProcessorState::Running) {
            ++count;
        }
    }
    return count;
}

void sleep_excess_processors(std::span<core::Processor* const> procs,
                             std::size_t target_active, int cstate) {
    std::size_t active = count_active_processors(procs);
    for (auto* proc : procs) {
        if (active <= target_active) {
            break;
        }
        if (proc->state() == core::ProcessorState::Idle) {
            proc->request_cstate(cstate);
            --active;
        }
    }
}

void apply_platform_target(EdfScheduler& /*scheduler*/, core::ClockDomain& domain,
                           const PlatformTarget& target, int sleep_cstate,
                           DvfsPolicy::FrequencyChangedCallback& on_freq_changed) {
    // DPM: sleep excess processors
    sleep_excess_processors(domain.processors(), target.active_processors, sleep_cstate);

    // DVFS: set frequency if different
    if (target.frequency != domain.frequency()) {
        domain.set_frequency(target.frequency);
        if (on_freq_changed) {
            on_freq_changed(domain);
        }
    }
}

} // namespace schedsim::algo::dvfs_dpm
