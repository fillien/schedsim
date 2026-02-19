#pragma once

/// @file dvfs_dpm_utils.hpp
/// @brief Utility functions and types for DVFS/DPM calculations.
/// @ingroup algo_policies

#include <schedsim/algo/dvfs_policy.hpp>

#include <schedsim/core/types.hpp>

#include <cstddef>
#include <span>

namespace schedsim::core {
class ClockDomain;
class Engine;
class Platform;
class Processor;
} // namespace schedsim::core

namespace schedsim::algo {

class EdfScheduler;

/// @brief Namespace for DVFS/DPM helper functions and types.
namespace dvfs_dpm {

/// @brief Combined DVFS+DPM target: a frequency and an active processor count.
///
/// Returned by policy computation functions and consumed by apply_platform_target().
///
/// @see apply_platform_target, FfaPolicy::compute_target, CsfPolicy::compute_target
struct PlatformTarget {
    core::Frequency frequency;        ///< Target clock frequency for the domain.
    std::size_t active_processors;    ///< Desired number of active (non-sleeping) processors.
};

/// @brief Compute the utilization scale factor for a heterogeneous platform domain.
///
/// Returns `ref_freq_max / (domain_freq_max * domain_perf)` so that raw task
/// utilization (WCET / Period) is converted to the fraction of a domain core's
/// capacity actually consumed. On a homogeneous platform this is 1.0.
///
/// @param platform The simulation platform (provides the reference frequency).
/// @param domain   The clock domain to compute the scale for.
/// @return The multiplicative scale factor.
///
/// @see EdfScheduler::utilization, compute_freq_min
double compute_utilization_scale(const core::Platform& platform,
                                 const core::ClockDomain& domain);

/// @brief Compute the minimum feasible frequency using the PA formula.
///
/// f_min = f_max * (U_total + (m - 1) * U_max) / m
///
/// This is the lowest frequency at which an m-processor global-EDF system
/// remains schedulable given aggregate utilization U_total and maximum
/// single-server utilization U_max.
///
/// @param freq_max   The maximum frequency of the clock domain (Hz).
/// @param total_util Aggregate active utilization (dimensionless, typically in [0, m]).
/// @param max_util   Maximum single-server utilization (dimensionless, in [0, 1]).
/// @param nb_procs   Number of processors in the domain.
/// @return The minimum feasible frequency (Hz, not yet clamped to hardware steps).
///
/// @see PowerAwareDvfsPolicy, FfaPolicy, CsfPolicy
double compute_freq_min(double freq_max, double total_util, double max_util, double nb_procs);

/// @brief Clamp a floating-point processor count to [1, max_procs].
///
/// @param value     The raw (possibly fractional) processor count.
/// @param max_procs The upper bound (total processors in the domain).
/// @return The clamped integer processor count, at least 1.
std::size_t clamp_procs(double value, std::size_t max_procs);

/// @brief Count processors in Idle or Running state.
///
/// Excludes processors in Sleep, Changing, or ContextSwitching states.
///
/// @param procs Span of processor pointers to inspect.
/// @return The number of currently active processors.
std::size_t count_active_processors(std::span<core::Processor* const> procs);

/// @brief Put excess idle processors to sleep.
///
/// Only transitions processors that are currently in the Idle state (never
/// Running). Iterates from the end of the span to avoid disturbing the
/// lowest-indexed active processors.
///
/// @param procs         Span of processor pointers in the domain.
/// @param target_active Desired number of active processors after sleeping.
/// @param cstate        C-state level to enter (e.g., 1 for C1).
/// @param engine        Optional engine pointer; if non-null, emits `proc_sleep`
///                      trace events.
///
/// @see apply_platform_target, DpmPolicy
void sleep_excess_processors(std::span<core::Processor* const> procs,
                             std::size_t target_active, int cstate,
                             core::Engine* engine = nullptr);

/// @brief Apply a computed platform target: DPM first (sleep excess), then DVFS (set frequency).
///
/// Coordinates the sleep_excess_processors() and ClockDomain::set_frequency()
/// calls, and invokes the frequency-changed callback so the scheduler can
/// update job completion timers.
///
/// @param scheduler       The EDF scheduler owning the affected processors.
/// @param domain          The clock domain to adjust.
/// @param target          The desired frequency and active processor count.
/// @param sleep_cstate    C-state for sleeping excess processors.
/// @param on_freq_changed Callback to invoke after the frequency change.
///
/// @see PlatformTarget, FfaPolicy, CsfPolicy
void apply_platform_target(EdfScheduler& scheduler, core::ClockDomain& domain,
                           const PlatformTarget& target, int sleep_cstate,
                           DvfsPolicy::FrequencyChangedCallback& on_freq_changed);

} // namespace dvfs_dpm
} // namespace schedsim::algo
