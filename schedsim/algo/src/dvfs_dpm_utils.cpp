#include <schedsim/algo/dvfs_dpm_utils.hpp>

#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace schedsim::algo::dvfs_dpm {

double compute_utilization_scale(const core::Platform& platform,
                                 const core::ClockDomain& domain) {
    if (domain.processors().empty()) {
        return 1.0;
    }

    // Find the maximum frequency across all clock domains (reference)
    double ref_freq_max = 0.0;
    for (std::size_t i = 0; i < platform.clock_domain_count(); ++i) {
        ref_freq_max = std::max(ref_freq_max, platform.clock_domain(i).freq_max().mhz);
    }
    if (ref_freq_max <= 0.0) {
        return 1.0;
    }

    double domain_perf = domain.processors()[0]->type().performance();
    double domain_freq_max = domain.freq_max().mhz;
    double denominator = domain_freq_max * domain_perf;
    if (denominator <= 0.0) {
        throw std::invalid_argument(
            "compute_utilization_scale: domain freq_max * performance must be positive");
    }

    return ref_freq_max / denominator;
}

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
                             std::size_t target_active, int cstate,
                             core::Engine* engine) {
    std::size_t active = count_active_processors(procs);
    for (auto* proc : procs) {
        if (active <= target_active) {
            break;
        }
        if (proc->state() == core::ProcessorState::Idle) {
            proc->request_cstate(cstate);
            if (engine) {
                engine->trace([&](core::TraceWriter& w) {
                    w.type("proc_sleep");
                    w.field("cpu", static_cast<uint64_t>(proc->id()));
                    w.field("cluster_id", static_cast<uint64_t>(proc->clock_domain().id()));
                });
            }
            --active;
        }
    }
}

void apply_platform_target(EdfScheduler& scheduler, core::ClockDomain& domain,
                           const PlatformTarget& target, int sleep_cstate,
                           DvfsPolicy::FrequencyChangedCallback& on_freq_changed) {
    // DPM: sleep excess processors
    sleep_excess_processors(domain.processors(), target.active_processors, sleep_cstate,
                            &scheduler.engine());

    // DVFS: set frequency if different
    if (target.frequency != domain.frequency()) {
        domain.set_frequency(target.frequency);
        if (on_freq_changed) {
            on_freq_changed(domain);
        }
    }
}

} // namespace schedsim::algo::dvfs_dpm
