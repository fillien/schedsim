#pragma once

#include <schedsim/algo/dvfs_policy.hpp>

#include <schedsim/core/types.hpp>

#include <cstddef>
#include <span>

namespace schedsim::core {
class ClockDomain;
class Engine;
class Processor;
} // namespace schedsim::core

namespace schedsim::algo {

class EdfScheduler;

namespace dvfs_dpm {

struct PlatformTarget {
    core::Frequency frequency;
    std::size_t active_processors;
};

// f_min = f_max * (U_total + (m-1)*U_max) / m
double compute_freq_min(double freq_max, double total_util, double max_util, double nb_procs);

// Clamp processor count to [1, max_procs]
std::size_t clamp_procs(double value, std::size_t max_procs);

// Count processors in Idle or Running state (excludes Sleep, Changing, ContextSwitching)
std::size_t count_active_processors(std::span<core::Processor* const> procs);

// Put excess Idle processors to sleep. Only sleeps Idle processors (not Running).
// If engine is non-null, emits proc_sleep trace events.
void sleep_excess_processors(std::span<core::Processor* const> procs,
                             std::size_t target_active, int cstate,
                             core::Engine* engine = nullptr);

// Apply a computed platform target: DPM (sleep excess) then DVFS (set frequency)
void apply_platform_target(EdfScheduler& scheduler, core::ClockDomain& domain,
                           const PlatformTarget& target, int sleep_cstate,
                           DvfsPolicy::FrequencyChangedCallback& on_freq_changed);

} // namespace dvfs_dpm
} // namespace schedsim::algo
